#pragma once
#ifndef DATA_MANAGE_THREAT_H
#define DATA_MANAGE_THREAT_H
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

#include "structures.h"
#include "voxelizer.h"

//TODO: start using paths as func arguments for all the load, unload functionality
struct ChunkLoadInfo {
    glm::ivec2 gridCoord;
    uint32_t resolution;
};

namespace fs = std::filesystem;

class DataManageThreat {
public:
    DataManageThreat(VkDevice &device, VkBuffer &stagingBuffer, VkDeviceMemory &stagingBufferMemory,
                     VkDeviceSize bufferSize,
                     VkQueue &transferQueue, uint32_t maxChunkResolution, uint32_t gridSize,
                     std::string objFile) : stopFlag(false), bufferSize(bufferSize),
                                            device(device),
                                            stagingBuffer(stagingBuffer),
                                            stagingBufferMemory(stagingBufferMemory),
                                            transferQueue(transferQueue),
                                            maxChunkResolution(maxChunkResolution),
                                            objFile(objFile),
                                            gridSize(gridSize) {
        workerThread = std::thread([this]() { this->threadLoop(); });
        fs::path filePath{objFile};
        this->objDirectory = filePath.parent_path().string();
        this->directory = std::format("{}_{}", (filePath.parent_path() / filePath.stem()).string(), gridSize);
        loadObj();
    }

    ~DataManageThreat() {
        // Signal the thread to stop and wake it up
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stopFlag = true;
        }
        cv.notify_all();
        workerThread.join();

        for (const auto &[key, value]: textures) {
            stbi_image_free(value.imageData);
        }
    }

    void pushWork(ChunkLoadInfo job) { {
            std::lock_guard<std::mutex> lock(queueMutex);
            workQueue.push(job);
        }
        cv.notify_one(); // wake the thread
    }

private:
    std::thread workerThread;
    std::queue<ChunkLoadInfo> workQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool stopFlag;

    VkDeviceSize bufferSize;
    VkDevice &device;
    VkBuffer &stagingBuffer;
    VkDeviceMemory &stagingBufferMemory;
    VkQueue &transferQueue;
    uint32_t maxChunkResolution;
    std::string objFile;
    std::string objDirectory;
    std::string directory;
    uint32_t gridSize;

    std::vector<TexturedTriangle> triangles;
    std::map<std::string, LoadedTexture> textures;

    uint32_t rootNodeIndex;
    uint32_t farValuesOffset;

    void loadObj() {
        int totalResolution = maxChunkResolution * gridSize;
        float _scale;
        int result = loadObject(objFile, objDirectory, totalResolution, gridSize, triangles, _scale);
    }


    void threadLoop() {
        while (true) {
            ChunkLoadInfo job; {
                std::unique_lock<std::mutex> lock(queueMutex);
                cv.wait(lock, [this] { return stopFlag || !workQueue.empty(); });

                if (stopFlag && workQueue.empty())
                    return; // exit thread

                job = workQueue.front();
                workQueue.pop();
            }

            // Do the work outside the lock
            loadChunkToGPU(job);
        }
    }

    void loadChunkToGPU(ChunkLoadInfo job) {
        //TODO: When loading a chunk we should check if the desired resolution is still what the job expects.
        std::cout << std::format("Chunk Coords to load: {}, {}, Desired resolution: {}", job.gridCoord.x,
                                 job.gridCoord.y,
                                 job.resolution) <<
                std::endl;

        auto chunkFarValues = std::vector<uint32_t>();
        auto chunkOctreeGPU = std::vector<uint32_t>();
        loadChunkData(job, chunkFarValues, chunkOctreeGPU);
        auto chunkGpu = Chunk{farValuesOffset, rootNodeIndex};

        VkDeviceSize farValuesSize = chunkFarValues.size() * sizeof(uint32_t);
        VkDeviceSize octreeSize = chunkOctreeGPU.size() * sizeof(uint32_t);
        VkDeviceSize chunkSize = sizeof(Chunk);
        //Copy the chunk Values into the staging buffer
        VkDeviceSize totalSize = farValuesSize + octreeSize + chunkSize;

        if (totalSize > bufferSize) {
            std::cerr << "Chunk values are bigger than staging buffer!" << std::endl;
            return;
        }

        void *data;
        vkMapMemory(device, stagingBufferMemory, 0, totalSize, 0, &data);

        auto *dst = reinterpret_cast<uint8_t *>(data);

        memcpy(dst, chunkFarValues.data(), (size_t) farValuesSize);
        dst += farValuesSize;
        memcpy(dst, chunkOctreeGPU.data(), (size_t) octreeSize);
        dst += octreeSize;
        memcpy(dst, &chunkGpu, (size_t) chunkSize);

        vkUnmapMemory(device, stagingBufferMemory);
    }

    void loadChunkData(ChunkLoadInfo &job, std::vector<uint32_t> &chunkFarValues,
                       std::vector<uint32_t> &chunkOctreeGPU) {
        uint32_t nodeAmount = 0;
        if (!loadChunk(directory, maxChunkResolution, job.resolution, job.gridCoord, nodeAmount, chunkOctreeGPU,
                       chunkFarValues)) {
            std::cout << "Chunk not yet created, generating the chunk" << std::endl;
            auto aabb = Aabb{};
            aabb.aa = glm::ivec3(job.gridCoord.x * maxChunkResolution, job.gridCoord.x * maxChunkResolution, 0);
            aabb.bb = glm::ivec3(aabb.aa.x + maxChunkResolution, aabb.aa.y + maxChunkResolution, maxChunkResolution);
            uint32_t maxDepth = std::ceil(std::log2(job.resolution));

            std::vector<uint32_t> allIndices(triangles.size());
            std::iota(allIndices.begin(), allIndices.end(), 0);

            auto node = createNode(aabb, triangles, allIndices, textures, nodeAmount, maxDepth, 0);
            if (node) {
                auto shared_node = std::make_shared<OctreeNode>(*node);
                addOctreeGPUdata(chunkOctreeGPU, shared_node, nodeAmount, chunkFarValues);
                if (!saveChunk(directory, maxChunkResolution, job.resolution, job.gridCoord, nodeAmount,
                               chunkOctreeGPU, chunkFarValues)) {
                    std::cout << "Something went wrong storing Chunk data" << std::endl;
                }
            } else {
                saveChunk(directory, maxChunkResolution, job.resolution, job.gridCoord, nodeAmount,
                          chunkOctreeGPU, chunkFarValues);
            }
        }
    }
};


//Check whether all currently loaded chunks are in the right resolution and queue them to be loaded if not
void checkChunks(std::vector<CpuChunk> &chunks, Camera &camera, uint32_t maxChunkResolution,
                 DataManageThreat &dmThreat) {
    auto cameraChunkCoords = glm::ivec2(floor(camera.position.x / maxChunkResolution),
                                        floor(camera.position.y / maxChunkResolution));
    //TODO: Schedule so that the chunks around the camera are checked / scheduled first
    uint32_t gridSize = sqrt(chunks.size());
    for (int chunkY = 0; chunkY < gridSize; chunkY++) {
        for (int chunkX = 0; chunkX < gridSize; chunkX++) {
            int dist = std::max(abs(chunkY - cameraChunkCoords.y), abs(chunkX - cameraChunkCoords.x));
            uint32_t octreeResolution = maxChunkResolution >> dist;
            auto gridCoord = glm::ivec2{chunkX, chunkY};

            CpuChunk &chunk = chunks[chunkY * gridSize + chunkX];
            if (chunk.resolution < octreeResolution && chunk.loading != true) {
                dmThreat.pushWork(ChunkLoadInfo{gridCoord, octreeResolution});
                chunk.loading = true;
            }
        }
    }
}

#endif //DATA_MANAGE_THREAT_H
