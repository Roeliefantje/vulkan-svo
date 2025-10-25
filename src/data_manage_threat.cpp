//
// Created by roeld on 22/10/2025.
//
#include "stb_image.h"
#include <iostream>
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>

#include "structures.h"
#include "voxelizer.h"
#include "data_manage_threat.h"


#include "svo_generation.h"

BufferManager::BufferManager(VkBuffer &buffer, VkDeviceSize bufferSize) : buffer(buffer), bufferSize(bufferSize) {
    this->chunks = std::vector<DataChunk>();
    //Initial offset is 1, so 0 can be reserved as a special value
    this->chunks.emplace_back(1, bufferSize - 1, false);
}

size_t BufferManager::allocateChunk(size_t size) {
    if (size == 0) {
        std::cerr << "Tried allocating 0 memory!" << std::endl;
        return 0;
    }

    auto best = chunks.end();
    size_t bestSize = bufferSize + 1;
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        if (!it->occupied && it->elementSize >= size && it->elementSize < bestSize) {
            best = it;
            bestSize = it->elementSize;
        }
    }

    if (best == chunks.end()) {
        std::cerr << "No possible location to allocate Chunk!" << std::endl;
        return 0;
    }

    size_t remaining_area = bestSize - size;
    size_t offset = best->offset;
    best->elementSize = size;
    best->occupied = true;
    if (remaining_area > 0) {
        chunks.emplace(std::next(best), offset + size, remaining_area, false);
    }


    return offset;
}

void BufferManager::freeChunk(size_t offset) {
    auto chunk = chunks.end();
    for (auto it = chunks.begin(); it != chunks.end(); ++it) {
        if (it->offset == offset) {
            chunk = it;
            break;
        }
    }

    if (chunk == chunks.end()) {
        std::cerr << "Tried to free an offset that is not allocated!" << std::endl;
        return;
    }

    //Check if prev and next are free, if so, merge them into one
    auto first = chunk;
    auto last = chunk;
    auto newSize = chunk->elementSize;
    if (chunk != chunks.begin() && !std::prev(chunk)->occupied) {
        first = std::prev(chunk);
        newSize += first->elementSize;
    }
    if (std::next(chunk) != chunks.end() && !std::next(chunk)->occupied) {
        last = std::next(chunk);
        newSize += last->elementSize;
    }
    first->occupied = false;
    first->elementSize = newSize;

    if (first != last) {
        //Next on first because we replace first not remove it, next on last because [__first,__last).
        chunks.erase(std::next(first), std::next(last));
    }
}


DataManageThreat::DataManageThreat(VkDevice &device, StagingBufferProperties &stagingBufferProperties, Config &config,
                                   BufferManager &octreeGPUManager,
                                   VkBuffer &chunkBuffer, BufferManager &farValuesManager,
                                   std::optional<SceneMetadata> objFileData,
                                   std::vector<CpuChunk> &chunks,
                                   Camera &camera)

    : stopFlag(false),
      config(config),
      stagingBufferProperties(stagingBufferProperties),
      chunks(chunks),
      camera(camera),
      device(device),
      octreeGPUManager(octreeGPUManager),
      farValuesManager(farValuesManager),
      chunkBuffer(chunkBuffer),
      objSceneData(objFileData) {
    std::cout << "Staging buffer size:" << stagingBufferProperties.bufferSize;
    workerThread = std::thread([this]() { this->threadLoop(); });
    if (objSceneData.has_value()) {
        objFile = objSceneData->objFile;
    } else {
        objFile = std::format("./assets/{}.obj", config.seed);
    }

    fs::path filePath{objFile};
    this->objDirectory = filePath.parent_path().string();
    this->directory = std::format("{}_{}", (filePath.parent_path() / filePath.stem()).string(), config.grid_size);
    // loadObj();
    initFence();
    initCommandBuffers();
}

DataManageThreat::~DataManageThreat() {
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

    vkDestroyFence(device, transferFence, nullptr);
    vkFreeCommandBuffers(device, stagingBufferProperties.transferCommandPool, 2, commandBuffers.data());
}

void DataManageThreat::pushWork(ChunkLoadInfo job) { {
        std::lock_guard<std::mutex> lock(queueMutex);
        workQueue.push(job);
    }
    cv.notify_one(); // wake the thread
}

bool DataManageThreat::CheckToWaitAndStartTransfer() {
    if (stagingBufferProperties.waitForTransfer) {
        //CommandBuffer is prepped and needs to be submitted
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffers[1];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &stagingBufferProperties.transferSemaphore;

        vkQueueSubmit(stagingBufferProperties.transferQueue, 1, &submitInfo, gridFence);
        //Update cpu side chunk info
        //Todo: queue old data for deletion
        auto chunk_idx = currentChunk.gridCoord.y * config.grid_size + currentChunk.gridCoord.x;
        CpuChunk &chunk = chunks[chunk_idx];
        chunk.loading = false;
        chunk.resolution = currentChunk.resolution;

        if (chunk.rootNodeIndex != 0) {
            octreeGPUManager.freeChunk(chunk.rootNodeIndex);
        }
        if (chunk.ChunkFarValuesOffset != 0) {
            farValuesManager.freeChunk(chunk.ChunkFarValuesOffset);
        }

        chunks[chunk_idx] = newChunk;

        //Tell data manage threat its okay to use staging buffer again.
        stagingBufferProperties.waitForTransfer.store(false);
        stagingBufferProperties.waitForTransfer.notify_one();


        return true;
    }

    return false;
}


void DataManageThreat::loadObj() {
    int totalResolution = config.chunk_resolution * config.grid_size;
    float _scale;
    int result = loadObject(objFile, objDirectory, totalResolution, config.grid_size, triangles, _scale);
}

void DataManageThreat::initFence() {
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = 0;
    vkCreateFence(device, &fenceInfo, nullptr, &transferFence);
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    vkCreateFence(device, &fenceInfo, nullptr, &gridFence);
}

void DataManageThreat::initCommandBuffers() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = stagingBufferProperties.transferCommandPool;
    allocInfo.commandBufferCount = 2;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data());
}

void DataManageThreat::threadLoop() {
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

void DataManageThreat::loadChunkToGPU(ChunkLoadInfo job) {
    std::cout << std::format("Chunk Coords to load: {}, {}, Desired resolution: {}", job.gridCoord.x,
                             job.gridCoord.y,
                             job.resolution) <<
            std::endl;
    if (!checkChunkResolution(job)) {
        //Chunk is not in the right resolution for the camera position, cancel
        chunks[job.gridCoord.y * config.grid_size + job.gridCoord.x].loading = false;
        return;
    }
    // std::this_thread::sleep_for(std::chrono::seconds(5));
    auto chunkOctreeGPU = std::vector<uint32_t>();
    auto chunkFarValues = std::vector<uint32_t>();
    loadChunkData(job, chunkFarValues, chunkOctreeGPU);
    uint32_t rootNodeIndex = 0;
    uint32_t farValuesOffset = 0;
    if (chunkOctreeGPU.size() > 0) {
        rootNodeIndex = octreeGPUManager.allocateChunk(chunkOctreeGPU.size());
        if (rootNodeIndex == 0) {
            std::cerr << "Octree GPU Buffer has no memory to be allocated!" << std::endl;
            // chunks[job.gridCoord.y * gridSize + job.gridCoord.x].loading = false;
            return;
        }
    }
    if (chunkFarValues.size() > 0) {
        farValuesOffset = farValuesManager.allocateChunk(chunkFarValues.size());
        if (farValuesOffset == 0) {
            std::cerr << "Far Values Buffer has no memory to be allocated!" << std::endl;
            chunks[job.gridCoord.y * config.grid_size + job.gridCoord.x].loading = false;
            return;
        }
    }
    auto chunkGpu = Chunk{farValuesOffset, rootNodeIndex};

    VkDeviceSize farValuesSize = chunkFarValues.size() * sizeof(uint32_t);
    VkDeviceSize octreeSize = chunkOctreeGPU.size() * sizeof(uint32_t);
    VkDeviceSize chunkSize = sizeof(Chunk);
    //Copy the chunk Values into the staging buffer
    VkDeviceSize totalSize = farValuesSize + octreeSize + chunkSize;

    if (totalSize > stagingBufferProperties.bufferSize) {
        std::cerr << "Chunk values are bigger than staging buffer!" << std::endl;
        return;
    }

    if (stagingBufferProperties.waitForTransfer) {
        std::cout << "Waiting for transfer" << std::endl;
        stagingBufferProperties.waitForTransfer.wait(true);
    }
    currentChunk = job;
    //Wait until all the data on the staging buffer is used before we use it again.
    vkWaitForFences(device, 1, &gridFence, VK_TRUE, UINT64_MAX);

    vkResetFences(device, 1, &gridFence);

    void *data;
    vkMapMemory(device, stagingBufferProperties.pStagingBufferMemory, 0, totalSize, 0, &data);

    auto *dst = reinterpret_cast<uint8_t *>(data);

    memcpy(dst, chunkFarValues.data(), (size_t) farValuesSize);
    dst += farValuesSize;
    memcpy(dst, chunkOctreeGPU.data(), (size_t) octreeSize);
    dst += octreeSize;
    memcpy(dst, &chunkGpu, (size_t) chunkSize);

    vkUnmapMemory(device, stagingBufferProperties.pStagingBufferMemory);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkResetCommandBuffer(commandBuffers[0], 0);
    vkBeginCommandBuffer(commandBuffers[0], &beginInfo);

    VkBufferCopy copyRegion{};
    // Copy octree
    if (octreeSize > 0) {
        // VkBufferCopy copyRegion{};
        copyRegion.srcOffset = farValuesSize;
        copyRegion.dstOffset = rootNodeIndex * sizeof(uint32_t);
        copyRegion.size = octreeSize;
        vkCmdCopyBuffer(commandBuffers[0], stagingBufferProperties.pStagingBuffer, octreeGPUManager.buffer, 1,
                        &copyRegion);
    }
    // Copy farvalues
    if (farValuesSize > 0) {
        copyRegion.srcOffset = 0;
        copyRegion.dstOffset = farValuesOffset * sizeof(uint32_t);
        copyRegion.size = farValuesSize;
        vkCmdCopyBuffer(commandBuffers[0], stagingBufferProperties.pStagingBuffer, farValuesManager.buffer, 1,
                        &copyRegion);
    }

    vkEndCommandBuffer(commandBuffers[0]);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffers[0];


    vkQueueSubmit(stagingBufferProperties.transferQueue, 1, &submitInfo, transferFence);

    //Wait for the octree copy to be done before we start copying grid values over.
    vkWaitForFences(device, 1, &transferFence, VK_TRUE, UINT64_MAX);
    vkResetFences(device, 1, &transferFence);

    //Start copying grid over
    vkResetCommandBuffer(commandBuffers[1], 0);
    vkBeginCommandBuffer(commandBuffers[1], &beginInfo);

    // Copy Grid
    copyRegion.srcOffset = farValuesSize + octreeSize;
    copyRegion.dstOffset = (job.gridCoord.y * config.grid_size + job.gridCoord.x) * sizeof(Chunk);
    copyRegion.size = chunkSize;

    vkCmdCopyBuffer(commandBuffers[1], stagingBufferProperties.pStagingBuffer, chunkBuffer, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffers[1]);
    newChunk = CpuChunk(farValuesOffset, rootNodeIndex, job.resolution);
    newChunk.chunkSize = chunkOctreeGPU.size();
    newChunk.offsetSize = chunkFarValues.size();
    stagingBufferProperties.waitForTransfer = true;
}


bool DataManageThreat::checkChunkResolution(ChunkLoadInfo &job) {
    auto cameraChunkCoords = glm::ivec2(floor(camera.position.x / config.chunk_resolution),
                                        floor(camera.position.y / config.chunk_resolution));
    uint32_t gridSize = sqrt(chunks.size());
    int dist = std::max(abs(job.gridCoord.y - cameraChunkCoords.y), abs(job.gridCoord.x - cameraChunkCoords.x));
    uint32_t octreeResolution = calculateChunkResolution(config.chunk_resolution, dist);

    return octreeResolution == job.resolution;
}

void DataManageThreat::loadChunkData(ChunkLoadInfo &job, std::vector<uint32_t> &chunkFarValues,
                                     std::vector<uint32_t> &chunkOctreeGPU) {
    uint32_t nodeAmount = 0;
    if (!loadChunk(directory, config.chunk_resolution, job.resolution, job.gridCoord, nodeAmount, chunkOctreeGPU,
                   chunkFarValues)) {
        if (!config.useHeightmapData && !sceneLoaded) {
            loadObj();
        }
        std::cout << "Chunk not yet created, generating the chunk" << std::endl;
        auto aabb = Aabb{};
        aabb.aa = glm::ivec3(job.gridCoord.x * config.chunk_resolution, job.gridCoord.y * config.chunk_resolution, 0);
        aabb.bb = glm::ivec3(aabb.aa.x + config.chunk_resolution, aabb.aa.y + config.chunk_resolution,
                             config.chunk_resolution);
        uint32_t maxDepth = std::ceil(std::log2(job.resolution));

        std::vector<uint32_t> allIndices(triangles.size());
        std::iota(allIndices.begin(), allIndices.end(), 0);
        std::optional<OctreeNode> node = std::nullopt;
        if (config.useHeightmapData) {
            uint32_t scale = config.chunk_resolution / job.resolution;
            node = createChunkOctree(job.resolution, config.seed, job.gridCoord, scale, nodeAmount);
        } else {
            node = createNode(aabb, triangles, allIndices, textures, nodeAmount, maxDepth, 0);
        }


        if (node) {
            auto shared_node = std::make_shared<OctreeNode>(*node);
            addOctreeGPUdata(chunkOctreeGPU, shared_node, nodeAmount, chunkFarValues);
            if (!saveChunk(directory, config.chunk_resolution, job.resolution, job.gridCoord, nodeAmount,
                           chunkOctreeGPU, chunkFarValues)) {
                std::cout << "Something went wrong storing Chunk data" << std::endl;
            }
        } else {
            saveChunk(directory, config.chunk_resolution, job.resolution, job.gridCoord, nodeAmount,
                      chunkOctreeGPU, chunkFarValues);
        }
    }
}


void checkChunks(std::vector<CpuChunk> &chunks, Camera &camera, uint32_t maxChunkResolution,
                 DataManageThreat &dmThreat) {
    auto cameraChunkCoords = glm::ivec2(floor(camera.position.x / maxChunkResolution),
                                        floor(camera.position.y / maxChunkResolution));
    //TODO: Schedule so that the chunks around the camera are checked / scheduled first
    uint32_t gridSize = sqrt(chunks.size());
    for (int chunkY = 0; chunkY < gridSize; chunkY++) {
        for (int chunkX = 0; chunkX < gridSize; chunkX++) {
            int dist = std::max(abs(chunkY - cameraChunkCoords.y), abs(chunkX - cameraChunkCoords.x));
            uint32_t octreeResolution = calculateChunkResolution(maxChunkResolution, dist);
            auto gridCoord = glm::ivec2{chunkX, chunkY};

            CpuChunk &chunk = chunks[chunkY * gridSize + chunkX];
            if (chunk.resolution != octreeResolution && chunk.loading != true) {
                dmThreat.pushWork(ChunkLoadInfo{gridCoord, octreeResolution});
                chunk.loading = true;
            }
        }
    }
}
