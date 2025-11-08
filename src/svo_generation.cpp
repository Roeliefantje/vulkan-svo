//
// Created by roeld on 22/10/2025.
//
#include "svo_generation.h"
#include <FastNoiseLite.h>

#include "spdlog/spdlog.h"

std::vector<float> createNoise(int chunkResolution, uint32_t seed_value, glm::ivec2 offset,
                               uint32_t maxChunkResolution) {
    float scale = maxChunkResolution / chunkResolution;
    spdlog::debug("Creating Heightmap");
    FastNoiseLite base;
    base.SetSeed((int) seed_value);
    base.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    base.SetFractalType(FastNoiseLite::FractalType_FBm);
    base.SetFractalLacunarity(2.0f);
    base.SetFractalGain(0.5f);
    base.SetFractalOctaves(6);
    base.SetFrequency(0.002f);

    int index = 0;
    std::vector<float> noiseData(chunkResolution * chunkResolution);
    for (int y = 0; y < chunkResolution; y++) {
        for (int x = 0; x < chunkResolution; x++) {
            //+ 100000 because negative values dont generate anything proper
            float fx = float(x * scale + offset.x + 100000), fy = float(y * scale + offset.y + 100000);
            float raw = base.GetNoise(fx, fy); // −1…+1
            float e = (raw + 1.0f) * 0.5f; //Make it always a positive height
            noiseData[index++] = e;
        }
    }
    spdlog::debug("Finished creating noise data");
    return noiseData;
}


std::vector<uint32_t> createDepthMap(std::vector<uint32_t> heightMap) {
    spdlog::debug("Creating Depthmap");
    std::vector<uint32_t> depthMap(heightMap.size());
    uint32_t size = std::sqrt(heightMap.size());
    spdlog::debug("Size: {}", size);

    //Could split it into a y scan and an x scan for faster performance.
    for (int y = 0; y < size; y++) {
        int base = y * size;
        for (int x = 0; x < size; x++) {
            uint32_t m = heightMap[base + x];
            if (y + 1 < size) m = std::min(m, heightMap[base + size + x]);
            if (y > 0) m = std::min(m, heightMap[base - size + x]);
            if (x + 1 < size) m = std::min(m, heightMap[base + x + 1]);
            if (x > 0) m = std::min(m, heightMap[base + x - 1]);
            depthMap[base + x] = m;
        }
    }

    spdlog::debug("Finished creating Depthmap");
    return depthMap;
}


std::optional<OctreeNode> createNode(int size, Aabb aabb, std::vector<float> &noise, uint32_t &nodeCount,
                                     int height_scale) {
    //Get aabb,
    auto node = OctreeNode();

    bool isSolid = true;
    bool isEmpty = true;
    bool shouldBreak = false;
    //Check full box
    for (int x = aabb.aa.x; x < aabb.bb.x; x++) {
        if (shouldBreak) break;
        for (int y = aabb.aa.y; y < aabb.bb.y; y++) {
            int z = noise[y * size + x] * height_scale;
            if (z >= aabb.aa.z) {
                isEmpty = false;
            }
            if (z < aabb.bb.z - 1) {
                isSolid = false;
            }
            if (!isSolid and !isEmpty) {
                shouldBreak = true;
                break;
            }
        }
    }

    if (isSolid) {
        node.color = getColor(noise[aabb.aa.y * size + aabb.aa.x], aabb.bb.x, aabb.bb.y);
        nodeCount++;
        return node;
    }

    if (isEmpty) {
        return std::nullopt;
    }

    glm::ivec3 midpoint = (aabb.aa + aabb.bb) / 2;
    //Check Children
    for (int z = 0; z < 2; z++) {
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 2; x++) {
                auto childaabb = Aabb{};
                childaabb.aa = glm::ivec3{
                    x == 0 ? aabb.aa.x : midpoint.x,
                    y == 0 ? aabb.aa.y : midpoint.y,
                    z == 0 ? aabb.aa.z : midpoint.z
                };
                childaabb.bb = glm::ivec3{
                    x == 0 ? midpoint.x : aabb.bb.x,
                    y == 0 ? midpoint.y : aabb.bb.y,
                    z == 0 ? midpoint.z : aabb.bb.z
                };


                auto child = createNode(size, childaabb, noise, nodeCount, height_scale);
                if (child) {
                    int childIndex = z * 4 + y * 2 + x;
                    node.childMask |= 1u << (7 - childIndex);
                    node.children[childIndex] = std::make_shared<OctreeNode>(*child);
                }
            }
        }
    }

    nodeCount++;
    return node;
}


std::optional<OctreeNode> createHollowNode(int size, Aabb aabb, std::vector<uint32_t> &heightMap,
                                           std::vector<uint32_t> &depthMap, uint32_t &nodeCount) {
    //Get aabb,
    auto node = OctreeNode();

    glm::ivec3 sizeVec = aabb.bb - aabb.aa;

    bool isSolid = true;
    bool isEmpty = true;
    bool shouldBreak = false;
    //Check full box
    for (int x = aabb.aa.x; x < aabb.bb.x; x++) {
        if (shouldBreak) break;
        for (int y = aabb.aa.y; y < aabb.bb.y; y++) {
            int z_max = heightMap[y * size + x];
            int z_min = depthMap[y * size + x];
            if (z_max >= aabb.aa.z && z_min < aabb.bb.z) {
                isEmpty = false;
            }
            if (z_max < aabb.bb.z - 1 || z_min > aabb.aa.z) {
                isSolid = false;
            }
            if (!isSolid and !isEmpty) {
                shouldBreak = true;
                break;
            }
        }
    }

    if (isSolid) {
        node.color = getColor(aabb.bb.z, aabb.bb.x, aabb.bb.y);
        nodeCount++;
        return node;
    }

    if (isEmpty) {
        return std::nullopt;
    }

    glm::ivec3 midpoint = (aabb.aa + aabb.bb) / 2;
    //Check Children
    for (int z = 0; z < 2; z++) {
        for (int y = 0; y < 2; y++) {
            for (int x = 0; x < 2; x++) {
                auto childaabb = Aabb{};
                childaabb.aa = glm::ivec3{
                    x == 0 ? aabb.aa.x : midpoint.x,
                    y == 0 ? aabb.aa.y : midpoint.y,
                    z == 0 ? aabb.aa.z : midpoint.z
                };
                childaabb.bb = glm::ivec3{
                    x == 0 ? midpoint.x : aabb.bb.x,
                    y == 0 ? midpoint.y : aabb.bb.y,
                    z == 0 ? midpoint.z : aabb.bb.z
                };


                auto child = createHollowNode(size, childaabb, heightMap, depthMap, nodeCount);
                if (child) {
                    int childIndex = z * 4 + y * 2 + x;
                    node.childMask |= 1u << (7 - childIndex);
                    node.children[childIndex] = std::make_shared<OctreeNode>(*child);
                }
            }
        }
    }

    nodeCount++;
    return node;
}

std::optional<OctreeNode> createChunkOctree(int chunkResolution, uint32_t seed_value, glm::ivec3 chunk_coords,
                                            int maxChunkResolution,
                                            int grid_height,
                                            uint32_t &nodeCount) {
    glm::vec2 offset = maxChunkResolution * chunk_coords;
    auto noise = createNoise(chunkResolution, seed_value, offset, maxChunkResolution);
    auto aabb = Aabb{};
    aabb.aa = glm::ivec3(0, 0, chunk_coords.z * maxChunkResolution);
    aabb.bb = glm::ivec3(chunkResolution, chunkResolution, aabb.aa.z + maxChunkResolution);
    int heightScale = maxChunkResolution * grid_height;

    auto node = createNode(chunkResolution, aabb, noise, nodeCount, heightScale);

    if (node) {
        return *node;
    }

    //Todo!: dont return leaf node if thing is empty
    return std::nullopt;
}

// OctreeNode createHollowOctree(int size, uint32_t seed_value, uint32_t &nodeCount) {
//     auto heightMap = createNoise(size, seed_value, glm::vec2(0), 1);
//     auto depthMap = createDepthMap(heightMap);
//     assert(heightMap.size() == size * size);
//     assert(depthMap.size() == size * size);
//
//     auto aabb = Aabb{};
//     aabb.aa = glm::ivec3(0, 0, 0);
//     aabb.bb = glm::ivec3(size, size, size);
//
//     std::cout << "Creating hollow nodes" << std::endl;
//     auto node = createHollowNode(size, aabb, heightMap, depthMap, nodeCount);
//     std::cout << "Finished creating hollow nodes" << std::endl;
//     if (node) {
//         return *node;
//     }
//
//     //Todo!: dont return leaf node if thing is empty
//     return OctreeNode();
// }

