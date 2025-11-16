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
#include "scene_metadata.h"
#include "config.h"

//TODO: start using paths as func arguments for all the load, unload functionality
struct ChunkLoadInfo {
    glm::ivec3 gridCoord;
    uint32_t resolution;
    glm::ivec3 chunkCoord;
};

namespace fs = std::filesystem;

// Computes discrete LOD level based on distance
inline uint32_t computeLOD(float distance) {
    distance = std::max(distance, 1.0f); // avoid log2(0)
    return static_cast<uint32_t>(std::floor(std::log2(distance)));
}

// Computes chunk resolution (in voxels per edge)
inline uint32_t calculateChunkResolution(uint32_t maxResolution, float distance) {
    uint32_t lod = computeLOD(distance);
    uint32_t resolution = maxResolution >> lod; // divide by 2^lod
    return std::min(1024u, std::max(resolution, 8u)); // clamp to some minimum
}

// inline uint32_t calculateChunkResolution(uint32_t maxChunkResolution, float dist) {
//     float distance = std::max(dist, 1.0f);
//     uint32_t lodLevel = uint32_t
//
//     // int lodLevel;
//     // if (dist < 2) lodLevel = 0;
//     // else if (dist < 5) lodLevel = 1;
//     // else if (dist < 10) lodLevel = 2;
//     // else if (dist < 20) lodLevel = 3;
//     // else lodLevel = 4;
//     //
//     // return std::max(maxChunkResolution >> lodLevel, 1u);
// }

class BufferManager {
public:
    VkBuffer &buffer;

    BufferManager(VkBuffer &buffer, VkDeviceSize bufferSize, const std::string &name, size_t itemSize);

    size_t allocateChunk(size_t size);

    void freeChunk(size_t offset);

    void printBufferInfo();

private:
    struct DataChunk {
        size_t offset;
        size_t elementSize;
        bool occupied;
    };

    //Buffer size in allowed elements, so if type is uin32_t, byte size would be bufferSize * sizeof(uint32_t)
    uint32_t bufferSize;
    std::vector<DataChunk> chunks;
    std::mutex mut;
    const std::string name;
    size_t itemSize;
};

struct TransferInformation {
    uint32_t chunk_idx;
    size_t staging_offset;
    CpuChunk newChunk;
};

class DataManageThreat {
public:
    DataManageThreat(VkDevice &device, StagingBufferProperties &stagingBufferProperties, Config &config,
                     BufferManager &octreeGPUManager, BufferManager &stagingBufferManager,
                     VkBuffer &chunkBuffer, BufferManager &farValuesManager,
                     std::optional<SceneMetadata> objFile,
                     std::vector<CpuChunk> &chunks,
                     CPUCamera &camera);

    ~DataManageThreat();

    void pushWork(ChunkLoadInfo job);

    bool CheckToWaitAndStartTransfer();

private:
    std::thread workerThread;
    std::queue<ChunkLoadInfo> workQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool stopFlag;

    Config &config;
    StagingBufferProperties &stagingBufferProperties;
    std::vector<CpuChunk> &chunks;
    CPUCamera &camera;
    VkDevice &device;
    VkCommandPool threadCommandPool;
    VkCommandBuffer threadCommandBuffer;
    // std::array<VkCommandBuffer, 2> commandBuffers;
    BufferManager &stagingBufferManager;
    BufferManager &octreeGPUManager;
    BufferManager &farValuesManager;
    VkBuffer &chunkBuffer;
    std::string objFile;
    std::string objDirectory;
    std::string directory;
    VkFence transferFence;
    VkFence gridFence;

    std::vector<TexturedTriangle> triangles;
    std::map<std::string, LoadedTexture> textures;

    glm::ivec2 cameraChunk;

    std::optional<SceneMetadata> objSceneData;

    void *gpuDataPointer;

    std::queue<TransferInformation> transferQueue;
    std::queue<TransferInformation> chunkDeleteQueue;
    std::mutex transferQueueMutex;

    bool sceneLoaded = false;

    void loadObj();

    void initFence();

    void initCommandBuffers();


    void threadLoop();

    void loadChunkToGPU(ChunkLoadInfo job);

    bool checkChunkResolution(const ChunkLoadInfo &job);

    void loadChunkData(ChunkLoadInfo &job, std::vector<uint32_t> &chunkFarValues,
                       std::vector<uint32_t> &chunkOctreeGPU);
};


//Check whether all currently loaded chunks are in the right resolution and queue them to be loaded if not
void checkChunks(std::vector<CpuChunk> &chunks, CPUCamera &camera, uint32_t maxChunkResolution, float voxelScale, float scaleDistance,
                 DataManageThreat &dmThreat);

#endif //DATA_MANAGE_THREAT_H
