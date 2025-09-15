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

struct ChunkLoadInfo {
    glm::ivec2 gridCoord;
    uint32_t resolution;
};

class DataManageThreat {
public:
    DataManageThreat(VkBuffer &stagingBuffer, VkDeviceMemory &stagingBufferMemory,
                     VkQueue &transferQueue, uint32_t maxChunkResolution) : stopFlag(false),
                                                                            stagingBuffer(stagingBuffer),
                                                                            stagingBufferMemory(stagingBufferMemory),
                                                                            transferQueue(transferQueue),
                                                                            maxChunkResolution(maxChunkResolution) {
        workerThread = std::thread([this]() { this->threadLoop(); });
    }

    ~DataManageThreat() {
        // Signal the thread to stop and wake it up
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stopFlag = true;
        }
        cv.notify_all();
        workerThread.join();
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

    VkBuffer &stagingBuffer;
    VkDeviceMemory &stagingBufferMemory;
    VkQueue &transferQueue;
    uint32_t maxChunkResolution;

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
            loadChunk(job);
        }
    }

    void loadChunk(ChunkLoadInfo job) {
        //TODO: When loading a chunk we should check if the desired resolution is still what the job expects.
        std::cout << std::format("Chunk Coords to load: {}, {}, Desired resolution: {}", job.gridCoord.x,
                                 job.gridCoord.y,
                                 job.resolution) <<
                std::endl;
    }
};

#endif //DATA_MANAGE_THREAT_H
