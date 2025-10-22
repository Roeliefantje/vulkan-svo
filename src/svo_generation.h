#pragma once

#ifndef SVO_GENERATION_H
#define SVO_GENERATION_H

#include <array>
#include "structures.h"
#include <optional>


inline uint32_t getColor(int height, int x, int y, int chunk_scale) {
    // Simple hash function for coordinate-based randomness
    uint32_t hash = ((x * 73856093) ^ (y * 19349663)) & 255;
    int jitter = (hash % 11) - 5; // Jitter between -5 and +5

    int h = height * chunk_scale + jitter;

    if (h > 480) {
        return 0xFFFFFF; // Snow
    } else if (h > 400) {
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

std::vector<uint32_t> createNoise(int size, uint32_t seed_value, glm::ivec2 offset, uint32_t scale);

std::vector<uint32_t> createDepthMap(std::vector<uint32_t> heightMap);

struct Aabb {
    glm::ivec3 aa;
    glm::ivec3 bb;
};


std::optional<OctreeNode> createNode(int size, Aabb aabb, std::vector<uint32_t> &noise, uint32_t &nodeCount,
                                     int chunk_scale);

std::optional<OctreeNode> createHollowNode(int size, Aabb aabb, std::vector<uint32_t> &heightMap,
                                           std::vector<uint32_t> &depthMap, uint32_t &nodeCount);


OctreeNode createChunkOctree(int size, uint32_t seed_value, glm::ivec2 chunk_coords, int chunk_scale,
                             uint32_t &nodeCount);

OctreeNode createOctree(int size, uint32_t seed_value, uint32_t &nodeCount);

OctreeNode createHollowOctree(int size, uint32_t seed_value, uint32_t &nodeCount);

void constructGrid(std::vector<Chunk> &gridValues, std::vector<uint32_t> &farValues, std::vector<uint32_t> &octreeGPU,
                   uint32_t maxChunkResolution, uint32_t gridSize, uint32_t seed);

#endif //SVO_GENERATION_H
