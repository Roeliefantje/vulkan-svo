//
// Created by roeld on 22/10/2025.
//
#include "svo_generation.h"
#include <FastNoiseLite.h>

#include "spdlog/spdlog.h"

// LOD-aware noise generator
struct LODNoise {
    FastNoiseLite noiseBase;
    int maxOctaves; // max octaves at highest resolution
    float baseFrequency; // base frequency in world units (meters)

    LODNoise(int seed, float baseFreq = 0.0001f, int octaves = 6) {
        baseFrequency = baseFreq;
        maxOctaves = octaves;

        noiseBase.SetSeed(seed);
        noiseBase.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
        noiseBase.SetFractalType(FastNoiseLite::FractalType_FBm);
        noiseBase.SetFractalLacunarity(2.0f);
        noiseBase.SetFractalGain(0.5f);
        noiseBase.SetFractalOctaves(maxOctaves);
        noiseBase.SetFrequency(baseFrequency);
    }

    // Sample noise at a given world position and voxel size (LOD)
    float Sample(float x, float y, float voxelSize) {
        // Compute maximum frequency allowed for this LOD (Nyquist)
        // We don’t want to sample features smaller than 2x voxel size
        float maxFreq = 1.0f / (2.0f * voxelSize);

        // Compute effective octaves for this LOD
        // Each octave doubles frequency: f_i = baseFreq * 2^i
        int effectiveOctaves = maxOctaves;
        // for (int i = maxOctaves - 1; i >= 0; i--) {
        //     float octaveFreq = baseFrequency * powf(2.0f, (float) i);
        //     if (octaveFreq > maxFreq) effectiveOctaves--;
        // }

        noiseBase.SetFractalOctaves(effectiveOctaves);

        // Sample at voxel center
        float sx = x + voxelSize * 0.5;
        float sy = y + voxelSize * 0.5;

        return (noiseBase.GetNoise(sx + 10000, sy + 10000) + 1.0f) / 2.0f;
    }
};

// inline FastNoiseLite getFastNoise(int seed_value) {
//     FastNoiseLite base;
//     base.SetSeed((int) seed_value);
//     base.SetNoiseType(FastNoiseLite::NoiseType_Perlin);
//     base.SetFractalType(FastNoiseLite::FractalType_FBm);
//     base.SetFractalLacunarity(2.0f);
//     base.SetFractalGain(0.5f);
//     base.SetFractalOctaves(8);
//     base.SetFrequency(0.001f);
//     return base;
// }

std::vector<float> createNoise(uint32_t chunkResolution, uint32_t maxChunkResolution, uint32_t seed_value,
                               glm::vec2 offset, float voxelSize) {
    LODNoise terrainNoise(seed_value);
    const float chunkScale = maxChunkResolution / chunkResolution;
    const float chunkVoxelSize = voxelSize * chunkScale;
    int index = 0;
    std::vector<float> noiseData(chunkResolution * chunkResolution);
    for (int voxel_y = 0; voxel_y < chunkResolution; voxel_y++) {
        const float fy = offset.y + voxel_y * chunkVoxelSize;
        for (int voxel_x = 0; voxel_x < chunkResolution; voxel_x++) {
            const float fx = offset.x + voxel_x * chunkVoxelSize;
            noiseData[index++] = terrainNoise.Sample(fx, fy, chunkVoxelSize);
        }
    }
    return noiseData;
}

std::vector<float> createPathNoise(int keyFrames, float distance, float voxelScale, uint32_t seed_value, glm::vec2 offset,
                                   glm::vec2 direction) {
    LODNoise terrainNoise(seed_value);
    // FastNoiseLite noise = getFastNoise(seed_value);
    std::vector<float> noiseData(keyFrames);
    float stepSize = distance / static_cast<float>(keyFrames);
    for (int i = 0; i < keyFrames; i++) {
        float fx = offset.x + direction.x * i * stepSize;
        float fy = offset.x + direction.x * i * stepSize;
        // float e = (raw + 1.0f) * 0.5f; //Make it always a positive height
        noiseData[i] = terrainNoise.Sample(fx, fy, voxelScale);;
    }

    spdlog::debug("Finished making path noise data");
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

std::optional<OctreeNode> createChunkOctree(uint32_t chunkResolution, uint32_t seed_value, glm::ivec3 chunk_coords,
                                            uint32_t maxChunkResolution,
                                            float voxelSize,
                                            uint32_t grid_height,
                                            uint32_t &nodeCount) {
    glm::vec2 offset = static_cast<float>(maxChunkResolution) * voxelSize * glm::vec2(chunk_coords.x, chunk_coords.y);
    auto noise = createNoise(chunkResolution, maxChunkResolution, seed_value, offset, voxelSize);
    auto aabb = Aabb{};
    aabb.aa = glm::ivec3(0, 0, chunk_coords.z * maxChunkResolution);
    aabb.bb = glm::ivec3(chunkResolution, chunkResolution, aabb.aa.z + maxChunkResolution);
    int heightScale = maxChunkResolution * grid_height;

    auto node = createNode(chunkResolution, aabb, noise, nodeCount, heightScale);

    if (node) {
        return *node;
    }

    return std::nullopt;
}

