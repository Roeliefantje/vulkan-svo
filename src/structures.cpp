#include "structures.h"

#include <format>
#include <queue>

#include "spdlog/spdlog.h"

CpuChunk::CpuChunk(uint32_t chunkFarValuesOffset, uint32_t rootIndex, uint32_t resolution, glm::ivec3 chunk_coords)
    : ChunkFarValuesOffset(chunkFarValuesOffset), rootNodeIndex(rootIndex), resolution(resolution),
      chunk_coords(chunk_coords) {
    loading = false;
    chunkSize = 0;
    offsetSize = 0;
}

Chunk::Chunk(uint32_t chunkFarValuesOffset, uint32_t rootIndex)
    : ChunkFarValuesOffset(chunkFarValuesOffset), rootNodeIndex(rootIndex) {
}

Camera::Camera(glm::vec3 pos, glm::vec3 direction, int screenWidth, int screenHeight, float fovRadian,
               glm::ivec3 camera_grid_pos)
    : position(pos), camera_grid_pos(camera_grid_pos), fov(fovRadian) {
    this->direction = glm::normalize(direction);
    spdlog::debug("Direction {}, {}, {}", direction.x, direction.y, direction.z);
    up = glm::vec3(0.0, 0.0, 1.0);
    resolution = glm::vec2(screenWidth, screenHeight);

    if (this->direction == up) {
        throw std::runtime_error("Camera up vector is the same as Camera direction!");
    }
}

//Config and width can be yoinked from config
CPUCamera::CPUCamera(glm::vec3 pos, glm::vec3 direction, int screenWidth, int screenHeight, float fovRadian,
                     Config &config) : absolute_location(pos) {
    glm::vec3 chunk_position = glm::mod(pos, glm::vec3(config.chunk_resolution));
    chunk_coords = glm::ivec3(pos) / int(config.chunk_resolution);

    glm::ivec3 camera_grid_pos = (glm::ivec3(config.grid_size) / 2);
    camera_grid_pos.z = chunk_coords.z;
    gridSize = config.grid_size;
    gridHeight = config.grid_height;
    maxChunkResolution = config.chunk_resolution;
    gpu_camera = Camera(chunk_position, direction, screenWidth, screenHeight, fovRadian, camera_grid_pos);
}

inline glm::ivec2 positive_mod(const glm::ivec2 &a, const glm::ivec2 &b) {
    return (a % b + b) % b;
}

inline glm::vec3 positive_mod(const glm::vec3 &a, const glm::vec3 &b) {
    return glm::mod(glm::mod(a, b) + b, b);
}

void CPUCamera::updatePosition(glm::vec3 posChange) {
    //TODO: Current method wont work if camera is moved multiple chunks in one frame.
    absolute_location += posChange;
    gpu_camera.position += posChange;
    glm::ivec3 highMask = glm::greaterThan(gpu_camera.position, glm::vec3(maxChunkResolution));
    glm::ivec3 lowMask = glm::lessThan(gpu_camera.position, glm::vec3(0));
    if (glm::any(glm::notEqual(lowMask, glm::ivec3(0))) || glm::any(glm::notEqual(highMask, glm::ivec3(0)))) {
        spdlog::debug("Prior camera grid pos: {}, {}, {}", gpu_camera.camera_grid_pos.x, gpu_camera.camera_grid_pos.y,
                      gpu_camera.camera_grid_pos.z);

        glm::ivec3 diff = highMask * 1 + lowMask * -1;
        gpu_camera.camera_grid_pos += diff;
        glm::ivec2 gridxy = positive_mod(glm::ivec2(gpu_camera.camera_grid_pos.x, gpu_camera.camera_grid_pos.y),
                                         glm::ivec2(gridSize));
        gpu_camera.camera_grid_pos.x = gridxy.x;
        gpu_camera.camera_grid_pos.y = gridxy.y;
        // gpu_camera.camera_grid_pos = positive_mod(gpu_camera.camera_grid_pos + diff, glm::ivec3(gridSize));
        gpu_camera.position = positive_mod(gpu_camera.position, glm::vec3(static_cast<float>(maxChunkResolution)));
        spdlog::debug("After camera grid pos: {}, {}, {}", gpu_camera.camera_grid_pos.x, gpu_camera.camera_grid_pos.y,
                      gpu_camera.camera_grid_pos.z);
        chunk_coords += diff;
    }
}

void CPUCamera::setPosition(glm::vec3 newPosition) {
    gpu_camera.position = positive_mod(newPosition, glm::vec3(maxChunkResolution));
    glm::ivec3 new_chunk_coords = glm::ivec3(glm::floor(newPosition / static_cast<float>(maxChunkResolution)));
    glm::ivec3 diff = new_chunk_coords - chunk_coords;

    gpu_camera.camera_grid_pos += diff;
    glm::ivec2 gridxy = positive_mod(glm::ivec2(gpu_camera.camera_grid_pos.x, gpu_camera.camera_grid_pos.y),
                                     glm::ivec2(gridSize));
    gpu_camera.camera_grid_pos.x = gridxy.x;
    gpu_camera.camera_grid_pos.y = gridxy.y;

    chunk_coords = new_chunk_coords;
}


OctreeNode::OctreeNode() {
    childMask = 0;
    color = 0x777777u;
}

OctreeNode::OctreeNode(uint8_t childMask, std::array<std::shared_ptr<OctreeNode>, 8> &children)
    : childMask(childMask), children(children), color(0) {
}

GridInfo::GridInfo(uint32_t res, uint32_t gridSize, uint32_t gridHeight)
    : sunPosition(glm::vec3(4000, 4000, 500)), resolution(res), bufferSize(0), gridSize(gridSize),
      gridHeight(gridHeight) {
}

VkVertexInputBindingDescription Vertex::getBindingDescription() {
    VkVertexInputBindingDescription bindingDescription{};
    bindingDescription.binding = 0;
    bindingDescription.stride = sizeof(Vertex);
    bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bindingDescription;
}

std::array<VkVertexInputAttributeDescription, 2> Vertex::getAttributeDescriptions() {
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

uint64_t make_key(uint16_t x, uint16_t y, uint16_t z) {
    return (static_cast<uint64_t>(x) << 32) |
           (static_cast<uint64_t>(y) << 16) |
           (static_cast<uint64_t>(z));
}

bool hasChildren(uint8_t childMask) {
    return childMask != 0;
}

uint32_t amountChildren(uint8_t childMask) {
    return std::popcount(childMask);
}

uint32_t createGPUData(uint8_t childMask, uint32_t color, uint32_t index, std::vector<uint32_t> &farValues) {
    if (childMask == 0) {
        return color & ((1 << 24) - 1);
    }
    bool isFarValue = false;
    uint32_t usedIndex = index;
    if (index > (1 << 23) - 1) {
        isFarValue = true;
        usedIndex = farValues.size();
        if (usedIndex > (1 << 23) - 1) {
            throw std::runtime_error("Too many far values, exceeds pointer for far value!");
        }
        // std::cout << "Used index: " << usedIndex << std::endl;
        farValues.push_back(index);
    }

    return ((uint32_t(childMask) & 0xFFu) << 24) |
           ((uint32_t(isFarValue) & 0x1u) << 23) |
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
            if (!child) throw std::runtime_error("Null child in octree");

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
    spdlog::debug("Nodes amount: {}", nodesAmount);
    auto data = std::vector<uint32_t>(nodesAmount);
    uint32_t index = 1;
    data[0] = addChildren(rootNode, &data, &index, 0, farValues);
    spdlog::debug("Total Nodes: {}", data.size());
    return data;
}

void addOctreeGPUdataBF(std::vector<uint32_t> &gpuData, std::shared_ptr<OctreeNode> rootNode, uint32_t nodesAmount,
                        std::vector<uint32_t> &farValues) {
    //ParentIndex is a bit confusing in this matter, it is actually the index of the current node.
    struct QueueNode {
        std::shared_ptr<OctreeNode> node;
        uint32_t parentIndex;
    };
    //Instead of the previous structure, we add all the nodes of the tree next to eachother.
    uint32_t startIndex = gpuData.size();
    gpuData.resize(startIndex + nodesAmount);
    // uint32_t index = startIndex + 1;
    std::queue<QueueNode> q;
    q.push({rootNode, startIndex});
    startIndex += 1;

    while (!q.empty()) {
        auto current = q.front();
        q.pop();

        std::shared_ptr<OctreeNode> node = current.node;
        uint32_t parentIndex = current.parentIndex;
        uint32_t index = startIndex; //The first index of the children, is where we currently are.
        uint32_t childCount = amountChildren(node->childMask);
        startIndex += childCount;

        uint32_t childOffset = 0;
        for (int i = 0; i < 8; ++i) {
            uint32_t bitIndex = 7 - i;
            if ((node->childMask >> bitIndex) & 1) {
                std::shared_ptr<OctreeNode> child = node->children[i];
                if (!child) throw std::runtime_error("Null child in octree");

                // (*data)[index + childOffset] = addChildren(child, data, startIndex, index + childOffset, farValues);
                //Add children to queue to process its children later.
                q.push({child, index + childOffset});
                uint32_t offsetCalc = node->childMask >> (8 - i);
                if (childOffset != std::popcount(offsetCalc)) {
                    throw std::runtime_error("Counting bits gives different value!");
                }
                childOffset += 1;
            }
        }

        gpuData[parentIndex] = createGPUData(node->childMask, node->color, index - parentIndex, farValues);
    }
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
    auto check = [&](size_t dx, size_t dy, size_t dz, uint8_t bit, int idx) {
        auto key = make_key(x + dx, y + dy, z + dz);
        if (sparseGrid->contains(key)) {
            *childMask |= bit;
            (*children)[idx] = (*sparseGrid)[key];
        }
    };

    check(0, 0, 0, 0b10000000, 0);
    check(1, 0, 0, 0b01000000, 1);
    check(0, 1, 0, 0b00100000, 2);
    check(1, 1, 0, 0b00010000, 3);
    check(0, 0, 1, 0b00001000, 4);
    check(1, 0, 1, 0b00000100, 5);
    check(0, 1, 1, 0b00000010, 6);
    check(1, 1, 1, 0b00000001, 7);
}

bool isPowerOfTwo(int n) {
    return n > 0 && (n & (n - 1)) == 0;
}

bool childrenSolid(std::array<std::shared_ptr<OctreeNode>, 8> *children) {
    for (int i = 0; i < 8; i++) {
        if ((*children)[i]->childMask != 0) return false;
    }
    return true;
}
