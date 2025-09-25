#pragma once

#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <vector>
#include <random>
#include <unordered_map>
#include <memory>
#include <bit>
#include <iomanip> // for std::hex, std::setw, std::setfill

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct DebugValues {
    alignas(4) uint32_t totalSteps;
    alignas(4) uint32_t maxSteps;
    uint32_t pad[2];
};

//CPU and GPU Chunk struct, cpu has some more values to allow management by the worker thread
struct CpuChunk {
    uint32_t ChunkFarValuesOffset;
    uint32_t rootNodeIndex;
    uint32_t resolution;
    //Values used to manage memory
    uint32_t chunkSize;
    uint32_t offsetSize;
    bool loading = false;

    CpuChunk() = default;

    CpuChunk(uint32_t chunkFarValuesOffset, uint32_t rootIndex, uint32_t resolution) : ChunkFarValuesOffset(
            chunkFarValuesOffset),
        rootNodeIndex(rootIndex), resolution(resolution) {
        chunkSize = 0;
        offsetSize = 0;
    };
};

struct Chunk {
    uint32_t ChunkFarValuesOffset;
    uint32_t rootNodeIndex;

    Chunk() = default;

    Chunk(uint32_t chunkFarValuesOffset, uint32_t rootIndex) : ChunkFarValuesOffset(chunkFarValuesOffset),
                                                               rootNodeIndex(rootIndex) {
    };
};

struct TexturedTriangle {
    glm::vec3 v[3];
    glm::vec2 uv[3];
    std::string diffuse_texname; // empty if none
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
    alignas(8) glm::vec2 resolution;
    alignas(4) float fov;

    Camera() = default;

    //Technically, adding fov to a vec3 would save some memory. (it could all fit in 3 vec4s)
    Camera(glm::vec3 pos, glm::vec3 lookAt, int screenWidth, int screenHeight, float fovRadian) : position(pos),
        fov(fovRadian) {
        direction = glm::normalize(lookAt - pos);
        up = glm::vec3(0.0, 0.0, 1.0);
        resolution = glm::vec2(screenWidth, screenHeight);

        if (direction == up) {
            throw std::runtime_error("Camera up vector is the same as Camera direction!");
        }
    }
};


struct OctreeNode {
    uint8_t childMask;
    std::array<std::shared_ptr<OctreeNode>, 8> children;
    uint32_t color;

    OctreeNode() {
        childMask = 0;
        // static std::default_random_engine rndEngine((unsigned) time(nullptr));
        // static std::uniform_int_distribution<int> dist(0, (1 << 24) - 1);
        // color = dist(rndEngine);
        color = 0x777777u;
        // std::cout << "Hex: 0x" << std::hex << color << std::endl;
    }

    OctreeNode(uint8_t childMask, std::array<std::shared_ptr<OctreeNode>, 8> &children) : childMask(childMask),
        children(children) {
        color = 0;
    };
};

struct GridInfo {
    alignas(16) glm::vec3 sunPosition;
    alignas(4) uint32_t resolution;
    alignas(4) uint32_t bufferSize;
    alignas(4) uint32_t gridSize;

    GridInfo(uint32_t res, uint32_t gridSize) : resolution(res), gridSize(gridSize) {
        bufferSize = 0;
        sunPosition = glm::vec3(4000, 4000, 500);
    }
};

struct Vertex {
    glm::vec2 position;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

//Key make thing for sparseGrid
inline uint64_t make_key(uint16_t x, uint16_t y, uint16_t z) {
    return (static_cast<uint64_t>(x) << 32) |
           (static_cast<uint64_t>(y) << 16) |
           (static_cast<uint64_t>(z));
}


inline bool hasChildren(uint8_t childMask) {
    return childMask != 0;
}

inline uint32_t amountChildren(uint8_t childMask) {
    return std::popcount(childMask);
}

inline uint32_t createGPUData(uint8_t childMask, uint32_t color, uint32_t index, std::vector<uint32_t> &farValues) {
    if (childMask == 0) {
        return color & (1 << 24) - 1;
    }

    bool far = false;
    uint32_t usedIndex = index;
    if (index > (1 << 23) - 1) {
        far = true;
        usedIndex = farValues.size();
        if (usedIndex > (1 << 23) - 1) {
            throw std::runtime_error("Too many far values, exceeds pointer for far value!");
        }
        std::cout << "Used index: " << usedIndex << std::endl;
        farValues.push_back(index);
    }
    // return (childMask << 24) | (uint32_t(far) << 23) | (usedIndex);
    return ((uint32_t(childMask) & 0xFFu) << 24) |
           ((uint32_t(far) & 0x1u) << 23) |
           (usedIndex & 0x007FFFFFu);
}

uint32_t addChildren(std::shared_ptr<OctreeNode> node, std::vector<uint32_t> *data, uint32_t *startIndex,
                     uint32_t parentIndex, std::vector<uint32_t> &farValues) {
    uint32_t index = *startIndex;
    uint32_t childOffset = 0;
    *startIndex += amountChildren(node->childMask);
    for (int i = 0; i < 8; ++i) {
        uint32_t bitIndex = 7 - i;
        if ((node->childMask >> bitIndex) & 1) {
            std::shared_ptr<OctreeNode> child = node->children[i];
            if (!child) {
                throw std::runtime_error("Null child in octree");
            }
            (*data)[index + childOffset] = addChildren(child, data, startIndex, index + childOffset, farValues);
            uint32_t offsetCalc = node->childMask >> (8 - i);
            if (childOffset != std::popcount(offsetCalc)) {
                throw std::runtime_error("Counting bits gives different value!");
            }
            childOffset += 1;
        }
    }

    return createGPUData(node->childMask, node->color, index - parentIndex, farValues);
}

std::vector<uint32_t> getOctreeGPUdata(std::shared_ptr<OctreeNode> rootNode, uint32_t nodesAmount,
                                       std::vector<uint32_t> &farValues) {
    std::cout << "Nodes amount: " << nodesAmount << std::endl;
    auto data = std::vector<uint32_t>(nodesAmount);
    uint32_t index = 1;
    data[0] = addChildren(rootNode, &data, &index, 0, farValues);

    std::cout << "Total nodes: " << data.size() << std::endl;

    return data;
}

void addOctreeGPUdata(std::vector<uint32_t> &gpuData, std::shared_ptr<OctreeNode> rootNode, uint32_t nodesAmount,
                      std::vector<uint32_t> &farValues) {
    uint32_t startIndex = gpuData.size();
    gpuData.resize(startIndex + nodesAmount);
    uint32_t index = startIndex + 1;

    gpuData[startIndex] = addChildren(rootNode, &gpuData, &index, startIndex, farValues);
}

void checkChildren(uint8_t *childMask, std::array<std::shared_ptr<OctreeNode>, 8> *children,
                   std::unordered_map<uint64_t, std::shared_ptr<OctreeNode> > *sparseGrid,
                   size_t x, size_t y, size_t z) {
    //Top left front
    auto key = make_key(x, y, z);
    if (sparseGrid->contains(key)) {
        *childMask |= 0b10000000;
        (*children)[0] = (*sparseGrid)[key];
    }
    //Top right front
    key = make_key(x + 1, y, z);
    if (sparseGrid->contains(key)) {
        *childMask |= 0b01000000;
        (*children)[1] = (*sparseGrid)[key];
    }
    //Top left back
    key = make_key(x, y + 1, z);
    if (sparseGrid->contains(key)) {
        *childMask |= 0b00100000;
        (*children)[2] = (*sparseGrid)[key];
    }
    //Top right back
    key = make_key(x + 1, y + 1, z);
    if (sparseGrid->contains(key)) {
        *childMask |= 0b00010000;
        (*children)[3] = (*sparseGrid)[key];
    }

    //Bottom left front
    key = make_key(x, y, z + 1);
    if (sparseGrid->contains(key)) {
        *childMask |= 0b00001000;
        (*children)[4] = (*sparseGrid)[key];
    }
    //Bottom right front
    key = make_key(x + 1, y, z + 1);
    if (sparseGrid->contains(key)) {
        *childMask |= 0b00000100;
        (*children)[5] = (*sparseGrid)[key];
    }
    //Bottom left back
    key = make_key(x, y + 1, z + 1);
    if (sparseGrid->contains(key)) {
        *childMask |= 0b00000010;
        (*children)[6] = (*sparseGrid)[key];
    }
    //Bottom right back
    key = make_key(x + 1, y + 1, z + 1);
    if (sparseGrid->contains(key)) {
        *childMask |= 0b00000001;
        (*children)[7] = (*sparseGrid)[key];
    }
}

inline bool isPowerOfTwo(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

inline bool childrenSolid(std::array<std::shared_ptr<OctreeNode>, 8> *children) {
    for (int i = 0; i < 8; i++) {
        if ((*children)[i]->childMask != 0) {
            return false;
        }
    }
    return true;
}

#endif //STRUCTURES_H
