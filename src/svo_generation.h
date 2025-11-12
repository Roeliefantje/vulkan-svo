#pragma once

#ifndef SVO_GENERATION_H
#define SVO_GENERATION_H

#include <array>
#include "structures.h"
#include <optional>


inline uint32_t getColor(float height, int x, int y) {
    // Simple hash function for coordinate-based randomness
    // uint32_t hash = ((x * 73856093) ^ (y * 19349663)) & 255;
    // int jitter = (hash % 11) - 5; // Jitter between -5 and +5

    float h = height;

    if (h > 0.7f) {
        return 0xFFFFFF; // Snow
    } else if (h > 0.1f) {
        return 0xD9D9D9; // Rock
    } else if (h > 220) {
        return 0x7F8386; // Gray rocky terrain
    } else if (h > 140) {
        return 0x3A5F0B; // Forest green
    } else if (h > 100) {
        return 0x6B8E23; // Olive green
    } else if (h > 80) {
        return 0x9ACD32; // Yellow-green
    } else if (h > 40) {
        return 0xADFF2F; // Lime green
    } else if (h > 5) {
        return 0xC2B280; // Sand
    } else {
        return 0x1E90FF; // Water
    }
}

std::vector<float> createNoise(int chunkResolution, uint32_t seed_value, glm::ivec2 offset,
                               uint32_t maxChunkResolution);

std::vector<float> createPathNoise(int pathLength, uint32_t seed_value, glm::ivec2 offset,
                                   uint32_t maxChunkResolution, glm::vec2 direction);

std::vector<uint32_t> createDepthMap(std::vector<uint32_t> heightMap);

struct Aabb {
    glm::ivec3 aa;
    glm::ivec3 bb;
};


std::optional<OctreeNode> createNode(int size, Aabb aabb, std::vector<float> &noise, uint32_t &nodeCount,
                                     int height_scale);

std::optional<OctreeNode> createHollowNode(int size, Aabb aabb, std::vector<uint32_t> &heightMap,
                                           std::vector<uint32_t> &depthMap, uint32_t &nodeCount);


std::optional<OctreeNode> createChunkOctree(int chunkResolution, uint32_t seed_value, glm::ivec3 chunk_coords,
                                            int maxChunkResolution,
                                            int grid_height,
                                            uint32_t &nodeCount);

// OctreeNode createHollowOctree(int size, uint32_t seed_value, uint32_t &nodeCount);


#endif //SVO_GENERATION_H
