//
// Created by roeld on 22/10/2025.
//
#include "svo_generation.h"
#include <FastNoiseLite.h>

std::vector<uint32_t> createNoise(int size, uint32_t seed_value, glm::ivec2 offset, uint32_t scale) {
    std::cout << "Creating Heightmap" << std::endl;
    FastNoiseLite base;
    base.SetSeed((int) seed_value);
    base.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
    base.SetFractalType(FastNoiseLite::FractalType_FBm);
    base.SetFractalLacunarity(2.0f);
    base.SetFractalGain(0.5f);
    base.SetFractalOctaves(6);
    base.SetFrequency(0.002f);

    int index = 0;
    std::vector<uint32_t> noiseData(size * size);
    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            float fx = float(x * scale + offset.x + 100000), fy = float(y * scale + offset.y + 100000);
            float raw = base.GetNoise(fx, fy); // −1…+1
            float e = (raw + 1.0f) * 0.5f; //Make it always a positive height
            noiseData[index++] = uint32_t(e * 1024) / scale;
        }
    }
    std::cout << "Finished creating noise data" << std::endl;
    return noiseData;
}


// std::vector<uint32_t> createNoise(int size, uint32_t seed_value, glm::ivec2 offset, uint32_t scale) {
//     std::cout << "Creating Heightmap" << std::endl;
//     FastNoiseLite base;
//     // choose ridged for sharp peaks
//     base.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
//     base.SetFractalType(FastNoiseLite::FractalType_Ridged);
//     base.SetSeed(seed_value);
//     base.SetFrequency(0.00025f);
//     base.SetFractalOctaves(7);
//     base.SetFractalLacunarity(2.2f);
//     base.SetFractalGain(0.3f);
//
//     FastNoiseLite warp;
//     warp.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
//     warp.SetFractalType(FastNoiseLite::FractalType_FBm);
//     warp.SetSeed(seed_value ^ 0x5f3759df);
//     warp.SetFrequency(0.008f);
//     warp.SetFractalOctaves(4);
//     warp.SetFractalGain(0.5f);
//     warp.SetDomainWarpAmp(40.0f);
//
//     std::vector<uint32_t> noiseData(size * size);
//     int index = 0;
//
//     for (int y = 0; y < size; y++) {
//         for (int x = 0; x < size; x++) {
//             //Get value between 0 and 2 and * 100
//             float fx = float(x * scale + offset.x), fy = float(y * scale + offset.y);
//             warp.DomainWarp(fx, fy);
//             float raw = base.GetNoise(fx, fy); // −1…+1
//             float e = (raw + 1.0f) * 0.5f;
//             e = pow(e * 1.05f, 2.0f); // redistribute: sharper peaks & flat valleys
//             // std::cout << "Noise Value: " << val << std::endl;
//             noiseData[index++] = uint32_t(e * 500) / scale;
//         }
//     }
//
//     std::cout << "Finished creating noise data" << std::endl;
//     return noiseData;
// }

std::vector<uint32_t> createDepthMap(std::vector<uint32_t> heightMap) {
    std::cout << "Creating Depthmap" << std::endl;
    std::vector<uint32_t> depthMap(heightMap.size());
    uint32_t size = std::sqrt(heightMap.size());
    std::cout << "Size: " << size << std::endl;

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

    std::cout << "Finished creating Depthmap" << std::endl;
    return depthMap;
}


std::optional<OctreeNode> createNode(int size, Aabb aabb, std::vector<uint32_t> &noise, uint32_t &nodeCount,
                                     int chunk_scale) {
    //Get aabb,
    auto node = OctreeNode();

    bool isSolid = true;
    bool isEmpty = true;
    bool shouldBreak = false;
    //Check full box
    for (int x = aabb.aa.x; x < aabb.bb.x; x++) {
        if (shouldBreak) break;
        for (int y = aabb.aa.y; y < aabb.bb.y; y++) {
            int z = noise[y * size + x];
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
        node.color = getColor(aabb.bb.z, aabb.bb.x, aabb.bb.y, chunk_scale);
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


                auto child = createNode(size, childaabb, noise, nodeCount, chunk_scale);
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
        node.color = getColor(aabb.bb.z, aabb.bb.x, aabb.bb.y, 1.0);
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

OctreeNode createChunkOctree(int size, uint32_t seed_value, glm::ivec2 chunk_coords, int chunk_scale,
                             uint32_t &nodeCount) {
    glm::vec2 offset = size * chunk_scale * chunk_coords;
    auto noise = createNoise(size, seed_value, offset, chunk_scale);
    auto aabb = Aabb{};
    aabb.aa = glm::ivec3(0, 0, 0);
    aabb.bb = glm::ivec3(size, size, size);

    auto node = createNode(size, aabb, noise, nodeCount, chunk_scale);

    if (node) {
        return *node;
    }

    //Todo!: dont return leaf node if thing is empty
    return OctreeNode();
}

OctreeNode createOctree(int size, uint32_t seed_value, uint32_t &nodeCount) {
    return createChunkOctree(size, seed_value, glm::ivec2(0), 1, nodeCount);
}

OctreeNode createHollowOctree(int size, uint32_t seed_value, uint32_t &nodeCount) {
    auto heightMap = createNoise(size, seed_value, glm::vec2(0), 1);
    auto depthMap = createDepthMap(heightMap);
    assert(heightMap.size() == size * size);
    assert(depthMap.size() == size * size);

    auto aabb = Aabb{};
    aabb.aa = glm::ivec3(0, 0, 0);
    aabb.bb = glm::ivec3(size, size, size);

    std::cout << "Creating hollow nodes" << std::endl;
    auto node = createHollowNode(size, aabb, heightMap, depthMap, nodeCount);
    std::cout << "Finished creating hollow nodes" << std::endl;
    if (node) {
        return *node;
    }

    //Todo!: dont return leaf node if thing is empty
    return OctreeNode();
}

void constructGrid(std::vector<Chunk> &gridValues, std::vector<uint32_t> &farValues, std::vector<uint32_t> &octreeGPU,
                   uint32_t maxChunkResolution, uint32_t gridSize, uint32_t seed) {
    gridValues = std::vector<Chunk>(gridSize * gridSize);
    farValues = std::vector<uint32_t>();
    octreeGPU = std::vector<uint32_t>();

    for (int y = 0; y < gridSize; y++) {
        for (int x = 0; x < gridSize; x++) {
            uint32_t nodeAmount = 0;
            auto octreeResolution = std::max(maxChunkResolution >> int(std::sqrt(std::max(y, x))), 64u);
            auto octreeScale = maxChunkResolution / octreeResolution;
            std::cout << "Octree resolution: " << octreeResolution << std::endl;
            auto node = std::make_shared<OctreeNode>(
                createChunkOctree(octreeResolution, seed, glm::ivec2(x, y), octreeScale, nodeAmount));
            uint32_t rootNodeIndex = octreeGPU.size();
            std::cout << "Root node Index: " << rootNodeIndex << std::endl;
            addOctreeGPUdata(octreeGPU, node, nodeAmount, farValues);
            gridValues[y * gridSize + x] = Chunk{maxChunkResolution, rootNodeIndex};
        }
    }
}
