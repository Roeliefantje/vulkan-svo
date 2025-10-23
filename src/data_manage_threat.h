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

//TODO: start using paths as func arguments for all the load, unload functionality
struct ChunkLoadInfo {
    glm::ivec2 gridCoord;
    uint32_t resolution;
};

namespace fs = std::filesystem;

inline uint32_t calculateChunkResolution(uint32_t maxChunkResolution, int dist) {
    return maxChunkResolution >> (dist);
}

class BufferManager {
public:
    VkBuffer &buffer;

    BufferManager(VkBuffer &buffer, VkDeviceSize bufferSize);

    size_t allocateChunk(size_t size);

    void freeChunk(size_t offset);

private:
    struct DataChunk {
        size_t offset;
        size_t elementSize;
        bool occupied;
    };

    //Buffer size in allowed elements, so if type is uin32_t, byte size would be bufferSize * sizeof(uint32_t)
    uint32_t bufferSize;
    std::vector<DataChunk> chunks;
};

class DataManageThreat {
public:
    DataManageThreat(VkDevice &device, VkCommandPool &cmdPool, VkBuffer &stagingBuffer, BufferManager &octreeGPUBuffer,
                     VkBuffer &chunkBuffer, BufferManager &farValuesBuffer,
                     VkDeviceMemory &stagingBufferMemory,
                     VkDeviceSize bufferSize,
                     VkQueue &transferQueue, uint32_t maxChunkResolution, uint32_t gridSize,
                     SceneMetadata objFile, VkFence &renderFence, VkSemaphore &transferSema,
                     std::atomic_bool &waitForTransfer, std::vector<CpuChunk> &chunks,
                     Camera &camera);

    ~DataManageThreat();

    void pushWork(ChunkLoadInfo job);

    bool CheckToWaitAndStartTransfer();

private:
    std::thread workerThread;
    std::queue<ChunkLoadInfo> workQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    bool stopFlag;

    std::atomic_bool &waitForTransfer;
    std::vector<CpuChunk> &chunks;
    Camera &camera;
    VkFence &renderingFence;
    VkFence transferFence;
    VkFence gridFence;
    VkSemaphore &transferSemaphore;
    VkCommandPool &commandPool;
    VkDeviceSize bufferSize;
    VkDevice &device;
    std::array<VkCommandBuffer, 2> commandBuffers;
    VkBuffer &stagingBuffer;
    BufferManager &octreeGPUBuffer;
    VkBuffer &chunkBuffer;
    BufferManager &farValuesBuffer;
    VkDeviceMemory &stagingBufferMemory;
    VkQueue &transferQueue;
    uint32_t maxChunkResolution;
    std::string objFile;
    std::string objDirectory;
    std::string directory;
    uint32_t gridSize;

    std::vector<TexturedTriangle> triangles;
    std::map<std::string, LoadedTexture> textures;

    ChunkLoadInfo currentChunk;
    CpuChunk newChunk;

    SceneMetadata objSceneData;
    bool sceneLoaded = false;

    void loadObj();

    void initFence();

    void initCommandBuffers();


    void threadLoop();

    void loadChunkToGPU(ChunkLoadInfo job);

    bool checkChunkResolution(ChunkLoadInfo &job);

    void loadChunkData(ChunkLoadInfo &job, std::vector<uint32_t> &chunkFarValues,
                       std::vector<uint32_t> &chunkOctreeGPU);
};


//Check whether all currently loaded chunks are in the right resolution and queue them to be loaded if not
void checkChunks(std::vector<CpuChunk> &chunks, Camera &camera, uint32_t maxChunkResolution,
                 DataManageThreat &dmThreat);

#endif //DATA_MANAGE_THREAT_H
