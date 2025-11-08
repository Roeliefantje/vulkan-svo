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
#include "spdlog/spdlog.h"

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
                                   CPUCamera &camera)

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
    spdlog::debug("Staging buffer size: {}", stagingBufferProperties.bufferSize);
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
        auto chunk_idx = (currentChunk.gridCoord.z * config.grid_size * config.grid_size) +
                         (currentChunk.gridCoord.y * config.grid_size) + currentChunk.gridCoord.x;
        CpuChunk &chunk = chunks[chunk_idx];
        // chunk.loading = false;
        // chunk.resolution = currentChunk.resolution;

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
    float _scale;
    int result = loadObject(objFile, objDirectory, config.chunk_resolution, config.grid_size, config.grid_height,
                            triangles, _scale);
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
        ChunkLoadInfo job{}; {
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
    if (config.printChunkDebug) {
        spdlog::debug("Chunk Coords to load: {}, {}, {}, Desired resolution: {}", job.chunkCoord.x,
                      job.chunkCoord.y, job.chunkCoord.z,
                      job.resolution);
    }

    if (!checkChunkResolution(job)) {
        //Chunk is not in the right resolution for the camera position, cancel
        chunks[job.gridCoord.z * config.grid_size * config.grid_size
               + job.gridCoord.y * config.grid_size
               + job.gridCoord.x].loading = false;
        return;
    }
    // std::this_thread::sleep_for(std::chrono::seconds(5));
    auto chunkOctreeGPU = std::vector<uint32_t>();
    auto chunkFarValues = std::vector<uint32_t>();
    loadChunkData(job, chunkFarValues, chunkOctreeGPU);
    uint32_t rootNodeIndex = 0;
    uint32_t farValuesOffset = 0;
    if (!chunkOctreeGPU.empty()) {
        rootNodeIndex = octreeGPUManager.allocateChunk(chunkOctreeGPU.size());
        if (rootNodeIndex == 0) {
            std::cerr << "Octree GPU Buffer has no memory to be allocated!" << std::endl;
            // chunks[job.gridCoord.y * config.grid_size + job.gridCoord.x].loading = false;
            return;
        }
    }
    if (!chunkFarValues.empty()) {
        farValuesOffset = farValuesManager.allocateChunk(chunkFarValues.size());
        if (farValuesOffset == 0) {
            spdlog::error("Far Values Buffer has no memory to be allocated!");
            // chunks[job.gridCoord.y * config.grid_size + job.gridCoord.x].loading = false;
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
        // std::cout << "Waiting for transfer" << std::endl;
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
    copyRegion.dstOffset = ((job.gridCoord.z * config.grid_size * config.grid_size) + (
                                job.gridCoord.y * config.grid_size) + job.gridCoord.x) * sizeof(Chunk);
    copyRegion.size = chunkSize;

    vkCmdCopyBuffer(commandBuffers[1], stagingBufferProperties.pStagingBuffer, chunkBuffer, 1, &copyRegion);
    vkEndCommandBuffer(commandBuffers[1]);
    newChunk = CpuChunk(farValuesOffset, rootNodeIndex, job.resolution, job.chunkCoord);
    newChunk.chunkSize = chunkOctreeGPU.size();
    newChunk.offsetSize = chunkFarValues.size();
    stagingBufferProperties.waitForTransfer = true;
}


bool DataManageThreat::checkChunkResolution(const ChunkLoadInfo &job) {
    auto cameraChunkCoords = camera.chunk_coords;
    glm::ivec3 diff = job.chunkCoord - cameraChunkCoords;
    int dist = std::max(abs(diff.x), std::max(abs(diff.y), abs(diff.z)));
    uint32_t octreeResolution = calculateChunkResolution(config.chunk_resolution, dist);

    return octreeResolution == job.resolution;
}

inline bool sceneInChunk(const Aabb &scene, const Aabb &chunk, const float &scale) {
    return (
        chunk.aa.x < (scene.bb.x * scale) &&
        chunk.bb.x >= (scene.aa.x * scale) &&
        chunk.aa.y < (scene.bb.y * scale) &&
        chunk.bb.y >= (scene.aa.y * scale) &&
        chunk.aa.z < (scene.bb.z * scale) &&
        chunk.bb.z >= (scene.aa.z * scale)
    );
}

void DataManageThreat::loadChunkData(ChunkLoadInfo &job, std::vector<uint32_t> &chunkFarValues,
                                     std::vector<uint32_t> &chunkOctreeGPU) {
    uint32_t nodeAmount = 0;
    if (!loadChunk(directory, config.chunk_resolution, job.resolution, job.chunkCoord, nodeAmount, chunkOctreeGPU,
                   chunkFarValues)) {
        if (!config.useHeightmapData && !sceneLoaded) {
            spdlog::debug("Loading scene");
            loadObj();
            sceneLoaded = true;
            spdlog::debug("Finished loading scene");
        }
        std::cout << "Chunk not yet created, generating the chunk" << std::endl;
        auto aabb = Aabb{};
        aabb.aa = glm::ivec3(job.chunkCoord.x * config.chunk_resolution, job.chunkCoord.y * config.chunk_resolution,
                             job.chunkCoord.z * config.chunk_resolution);
        aabb.bb = glm::ivec3(aabb.aa.x + config.chunk_resolution, aabb.aa.y + config.chunk_resolution,
                             aabb.aa.z + config.chunk_resolution);
        uint32_t maxDepth = std::ceil(std::log2(job.resolution));

        std::vector<uint32_t> allIndices(triangles.size());
        std::iota(allIndices.begin(), allIndices.end(), 0);
        std::optional<OctreeNode> node = std::nullopt;
        if (config.useHeightmapData) {
            uint32_t scale = config.chunk_resolution / job.resolution;
            node = createChunkOctree(job.resolution, config.seed, job.chunkCoord, scale, nodeAmount);
        } else if (sceneInChunk(objSceneData->sceneAabb, aabb, objSceneData->scale)) {
            node = createNode(aabb, triangles, allIndices, textures, nodeAmount, maxDepth, 0);
        }


        if (node) {
            auto shared_node = std::make_shared<OctreeNode>(*node);
            addOctreeGPUdata(chunkOctreeGPU, shared_node, nodeAmount, chunkFarValues);
            if (!saveChunk(directory, config.chunk_resolution, job.resolution, job.chunkCoord, nodeAmount,
                           chunkOctreeGPU, chunkFarValues)) {
                std::cout << "Something went wrong storing Chunk data" << std::endl;
            }
        } else {
            saveChunk(directory, config.chunk_resolution, job.resolution, job.chunkCoord, nodeAmount,
                      chunkOctreeGPU, chunkFarValues);
        }
    }
}

inline int positive_mod(int a, int b) {
    return (a % b + b) % b;
}


void checkChunks(std::vector<CpuChunk> &chunks, CPUCamera &camera, uint32_t maxChunkResolution,
                 DataManageThreat &dmThreat) {
    auto center = camera.gpu_camera.camera_grid_pos;
    auto processOffset = [&](int dx, int dy, int dz) {
        if ((center.z + dz) < 0 || (center.z + dz) >= static_cast<int>(camera.gridHeight)) {
            return;
        }

        auto gridCoord = glm::ivec3{
            positive_mod(center.x + dx, static_cast<int>(camera.gridSize)),
            positive_mod(center.y + dy, static_cast<int>(camera.gridSize)),
            center.z + dz
        };

        auto chunkCoord = glm::ivec3{
            camera.chunk_coords.x + dx,
            camera.chunk_coords.y + dy,
            center.z + dz
        };

        auto dist = glm::ivec3(abs(dx), abs(dy), abs(dz));
        uint32_t octreeResolution = calculateChunkResolution(maxChunkResolution,
                                                             glm::max(dist.x, glm::max(dist.y, dist.z)));

        CpuChunk &chunk = chunks[gridCoord.z * camera.gridSize * camera.gridSize +
                                 gridCoord.y * camera.gridSize
                                 + gridCoord.x];
        if (!chunk.loading && (chunk.chunk_coords != chunkCoord || chunk.resolution != octreeResolution)) {
            dmThreat.pushWork(ChunkLoadInfo{gridCoord, octreeResolution, chunkCoord});
            chunk.loading = true;
        }
    };

    int rd = int((camera.gridSize - 1) / 2);

    for (int chunk_distance = 0; chunk_distance <= std::max(rd, int(camera.gridHeight));
         chunk_distance++) {
        for (int dz = -chunk_distance; dz <= chunk_distance; ++dz) {
            if (dz > int(camera.gridHeight)) {
                break;
            }
            for (int dy = -chunk_distance; dy <= chunk_distance; ++dy) {
                if (abs(dy) > rd) {
                    continue;
                }
                for (int dx = -chunk_distance; dx <= chunk_distance; ++dx) {
                    if (abs(dx) > rd) {
                        continue;
                    }
                    // Only process the *outer shell* at this distance
                    if (abs(dx) != chunk_distance &&
                        abs(dy) != chunk_distance &&
                        abs(dz) != chunk_distance)
                        continue;

                    processOffset(dx, dy, dz);
                }
            }
        }
    }
}
