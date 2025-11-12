#pragma once
#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <vector>
#include <random>
#include <unordered_map>
#include <memory>
#include <bit>
#include <iomanip>
#include <array>
#include <iostream>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/integer.hpp>

// Vulkan types
#include <atomic>
#include <vulkan/vulkan.h>

#include "config.h"

struct DebugValues {
    alignas(4) uint32_t totalSteps;
    alignas(4) uint32_t maxSteps;
    uint32_t pad[2];
};

struct StagingBufferProperties {
    VkCommandPool transferCommandPool; //The command pool used to submit transfer commands for the staging buffer.
    VkQueue transferQueue;
    std::mutex queueMut;
    VkBuffer pStagingBuffer;
    VkDeviceMemory pStagingBufferMemory;
    VkSemaphore transferSemaphore;
    std::atomic_bool waitForTransfer = false;
    VkDeviceSize bufferSize;
    uint32_t transferQueueIndex;
    VkCommandBuffer transferCommandBuffer;
};

struct CpuChunk {
    uint32_t ChunkFarValuesOffset = 0;
    uint32_t rootNodeIndex = 0;
    uint32_t resolution = 0;
    uint32_t chunkSize = 0;
    uint32_t offsetSize = 0;
    glm::ivec3 chunk_coords = glm::ivec3{0, 0, 0};
    bool loading = false;

    CpuChunk() = default;

    CpuChunk(uint32_t chunkFarValuesOffset, uint32_t rootIndex, uint32_t resolution, glm::ivec3 chunk_coords);
};

struct Chunk {
    uint32_t ChunkFarValuesOffset;
    uint32_t rootNodeIndex;

    Chunk() = default;

    Chunk(uint32_t chunkFarValuesOffset, uint32_t rootIndex);
};

struct TexturedTriangle {
    glm::vec3 v[3];
    glm::vec2 uv[3];
    std::string diffuse_texname;
    glm::vec3 diffuse;
};

struct LoadedTexture {
    unsigned char *imageData;
    int texWidth, texHeight, channels;
};

struct Camera {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 up;
    alignas(16) glm::ivec3 camera_grid_pos;
    alignas(8) glm::vec2 resolution;
    alignas(4) float fov;

    Camera() = default;

    Camera(glm::vec3 pos, glm::vec3 direction, int screenWidth, int screenHeight, float fovRadian,
           glm::ivec3 camera_grid_pos);
};

struct CPUCamera {
    Camera gpu_camera;
    glm::vec3 absolute_location;
    glm::ivec3 chunk_coords;
    uint32_t gridSize;
    uint32_t gridHeight;
    uint32_t maxChunkResolution;

    CPUCamera() = default;

    CPUCamera(glm::vec3 pos, glm::vec3 lookAt, int screenWidth, int screenHeight, float fovRadian,
              Config &config);

    void updatePosition(glm::vec3 posChange);

    void setPosition(glm::vec3 newPosition);
};

struct OctreeNode {
    uint8_t childMask;
    std::array<std::shared_ptr<OctreeNode>, 8> children;
    uint32_t color;

    OctreeNode();

    OctreeNode(uint8_t childMask, std::array<std::shared_ptr<OctreeNode>, 8> &children);
};

struct GridInfo {
    alignas(16) glm::vec3 sunPosition;
    alignas(4) uint32_t resolution;
    alignas(4) uint32_t bufferSize;
    alignas(4) uint32_t gridSize;
    alignas(4) uint32_t gridHeight;

    GridInfo() = default;

    GridInfo(uint32_t res, uint32_t gridSize, uint32_t gridHeight);
};

struct Vertex {
    glm::vec2 position;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription();

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions();
};

// Utility functions
uint64_t make_key(uint16_t x, uint16_t y, uint16_t z);

bool hasChildren(uint8_t childMask);

uint32_t amountChildren(uint8_t childMask);

uint32_t createGPUData(uint8_t childMask, uint32_t color, uint32_t index, std::vector<uint32_t> &farValues);

uint32_t addChildren(std::shared_ptr<OctreeNode> node, std::vector<uint32_t> *data, uint32_t *startIndex,
                     uint32_t parentIndex, std::vector<uint32_t> &farValues);

std::vector<uint32_t> getOctreeGPUdata(std::shared_ptr<OctreeNode> rootNode, uint32_t nodesAmount,
                                       std::vector<uint32_t> &farValues);

void addOctreeGPUdataBF(std::vector<uint32_t> &gpuData, std::shared_ptr<OctreeNode> rootNode, uint32_t nodesAmount,
                        std::vector<uint32_t> &farValues);

void addOctreeGPUdata(std::vector<uint32_t> &gpuData, std::shared_ptr<OctreeNode> rootNode, uint32_t nodesAmount,
                      std::vector<uint32_t> &farValues);

void checkChildren(uint8_t *childMask, std::array<std::shared_ptr<OctreeNode>, 8> *children,
                   std::unordered_map<uint64_t, std::shared_ptr<OctreeNode> > *sparseGrid,
                   size_t x, size_t y, size_t z);

bool isPowerOfTwo(int n);

bool childrenSolid(std::array<std::shared_ptr<OctreeNode>, 8> *children);

#endif // STRUCTURES_H
