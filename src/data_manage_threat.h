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
    DataManageThreat(VkDevice &device, StagingBufferProperties &stagingBufferProperties, Config &config,
                     BufferManager &octreeGPUManager,
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
    std::array<VkCommandBuffer, 2> commandBuffers;
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

    ChunkLoadInfo currentChunk;
    CpuChunk newChunk;
    glm::ivec2 cameraChunk;

    std::optional<SceneMetadata> objSceneData;
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
void checkChunks(std::vector<CpuChunk> &chunks, CPUCamera &camera, uint32_t maxChunkResolution,
                 DataManageThreat &dmThreat);

#endif //DATA_MANAGE_THREAT_H
