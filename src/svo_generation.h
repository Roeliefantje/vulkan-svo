#pragma once

#ifndef SVO_GENERATION_H
#define SVO_GENERATION_H

#include <array>
#include <FastNoiseLite.h>
#include "structures.h"
#include <optional>

std::vector<uint32_t> createNoise(int size, uint32_t seed_value) {
    std::cout << "Creating Heightmap" << std::endl;
    FastNoiseLite base;
    // choose ridged for sharp peaks
    base.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    base.SetFractalType(FastNoiseLite::FractalType_Ridged);
    base.SetSeed(seed_value);
    base.SetFrequency(0.00025f);
    base.SetFractalOctaves(7);
    base.SetFractalLacunarity(2.2f);
    base.SetFractalGain(0.3f);

    FastNoiseLite warp;
    warp.SetNoiseType(FastNoiseLite::NoiseType_OpenSimplex2);
    warp.SetFractalType(FastNoiseLite::FractalType_FBm);
    warp.SetSeed(seed_value ^ 0x5f3759df);
    warp.SetFrequency(0.008f);
    warp.SetFractalOctaves(4);
    warp.SetFractalGain(0.5f);
    warp.SetDomainWarpAmp(40.0f);

    std::vector<uint32_t> noiseData(size * size);
    int index = 0;

    for (int y = 0; y < size; y++) {
        for (int x = 0; x < size; x++) {
            //Get value between 0 and 2 and * 100
            float fx = float(x), fy = float(y);
            warp.DomainWarp(fx, fy);
            float raw = base.GetNoise(fx, fy); // −1…+1
            float e = (raw + 1.0f) * 0.5f;
            e = pow(e * 1.05f, 2.0f); // redistribute: sharper peaks & flat valleys
            // std::cout << "Noise Value: " << val << std::endl;
            noiseData[index++] = uint32_t(e * 500);
        }
    }

    std::cout << "Finished creating noise data" << std::endl;
    return noiseData;
}

struct Aabb {
    glm::ivec3 aa;
    glm::ivec3 bb;
};


std::optional<OctreeNode> createNode(int size, Aabb aabb, std::vector<uint32_t> &noise, uint32_t &nodeCount) {
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


                auto child = createNode(size, childaabb, noise, nodeCount);
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


OctreeNode createOctree(int size, uint32_t seed_value, uint32_t &nodeCount) {
    auto noise = createNoise(size, seed_value);
    auto aabb = Aabb{};
    aabb.aa = glm::ivec3(0, 0, 0);
    aabb.bb = glm::ivec3(size, size, size);

    auto node = createNode(size, aabb, noise, nodeCount);

    if (node) {
        return *node;
    }

    //Todo!: dont return leaf node if thing is empty
    return OctreeNode();
}

#endif //SVO_GENERATION_H
