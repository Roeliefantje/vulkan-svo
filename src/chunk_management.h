#pragma once


#ifndef CHUNK_MANAGEMENT_H
#define CHUNK_MANAGEMENT_H
#include <filesystem>
#include <fstream>
#include <iostream>
#include <format>


#include "data_manage_threat.h"
#include "structures.h"

bool saveChunk(const std::string &scenePath, uint32_t max_resolution, uint32_t svo_resolution, glm::ivec2 gridCoords,
               uint32_t &nodeCount,
               std::vector<uint32_t> &gpuData, std::vector<uint32_t> &farValues) {
    namespace fs = std::filesystem;

    try {
        fs::path dirPath(scenePath);
        auto max_resolution_string = std::format("max_scene_resolution_{}", max_resolution);
        auto chunk_resolution_string = std::format("chunk_resolution_{}", svo_resolution);
        auto file_name = std::format("x{},y{}.svo", gridCoords.x, gridCoords.y);
        fs::path fullDirPath = dirPath / max_resolution_string / chunk_resolution_string;
        fs::create_directories(fullDirPath);
        fs::path filePath = fullDirPath / file_name;

        std::ofstream outFile(filePath, std::ios::binary);
        if (!outFile) return false;
        // Write metadata
        outFile.write(reinterpret_cast<char *>(&nodeCount), sizeof(nodeCount));
        //Write Vector sizes to interpret reset of file
        uint32_t gpuDataSize = gpuData.size();
        uint32_t farValuesSize = farValues.size();
        outFile.write(reinterpret_cast<char *>(&gpuDataSize), sizeof(gpuDataSize));
        outFile.write(reinterpret_cast<char *>(&farValuesSize), sizeof(farValuesSize));

        // Write vectors
        outFile.write(reinterpret_cast<char *>(gpuData.data()), gpuDataSize * sizeof(uint32_t));
        outFile.write(reinterpret_cast<char *>(farValues.data()), farValuesSize * sizeof(uint32_t));

        outFile.close();
        return true;
    } catch (...) {
        return false;
    }
}

bool loadChunk(const std::string &scenePath, uint32_t max_resolution, uint32_t svo_resolution, glm::ivec2 gridCoords,
               uint32_t &nodeCount,
               std::vector<uint32_t> &gpuData, std::vector<uint32_t> &farValues) {
    namespace fs = std::filesystem;

    try {
        fs::path dirPath(scenePath);
        auto max_resolution_string = std::format("max_scene_resolution_{}", max_resolution);
        auto chunk_resolution_string = std::format("chunk_resolution_{}", svo_resolution);
        auto file_name = std::format("x{},y{}.svo", gridCoords.x, gridCoords.y);
        fs::path fullDirPath = dirPath / max_resolution_string / chunk_resolution_string;
        fs::create_directories(fullDirPath);
        fs::path filePath = fullDirPath / file_name;

        std::ifstream outFile(filePath, std::ios::binary);
        if (!outFile) return false;

        outFile.read(reinterpret_cast<char *>(&nodeCount), sizeof(nodeCount));

        // Read vector sizes
        uint32_t gpuDataSize, farValuesSize;
        outFile.read(reinterpret_cast<char *>(&gpuDataSize), sizeof(gpuDataSize));
        outFile.read(reinterpret_cast<char *>(&farValuesSize), sizeof(farValuesSize));

        // Read vectors
        size_t old_gpu_data_size = gpuData.size();
        size_t old_far_values_size = farValues.size();

        gpuData.resize(gpuDataSize + old_gpu_data_size);
        farValues.resize(farValuesSize + old_far_values_size);
        outFile.read(reinterpret_cast<char *>(gpuData.data() + old_gpu_data_size), gpuDataSize * sizeof(uint32_t));
        outFile.read(reinterpret_cast<char *>(farValues.data() + old_far_values_size),
                     farValuesSize * sizeof(uint32_t));

        outFile.close();

        return true;
    } catch (...) {
        return false;
    }
}


//Check whether all currently loaded chunks are in the right resolution and queue them to be loaded if not
void checkChunks(std::vector<CpuChunk> &chunks, Camera &camera, uint32_t maxChunkResolution,
                 DataManageThreat &dmThreat) {
    auto cameraChunkCoords = glm::ivec2(floor(camera.position.x / maxChunkResolution),
                                        floor(camera.position.y / maxChunkResolution));
    //TODO: Schedule so that the chunks around the camera are checked / scheduled first
    uint32_t gridSize = sqrt(chunks.size());
    for (int chunkY = 0; chunkY < gridSize; chunkY++) {
        for (int chunkX = 0; chunkX < gridSize; chunkX++) {
            int dist = std::max(abs(chunkY - cameraChunkCoords.y), abs(chunkX - cameraChunkCoords.x));
            uint32_t octreeResolution = maxChunkResolution >> dist;
            auto gridCoord = glm::ivec2{chunkX, chunkY};

            CpuChunk &chunk = chunks[chunkY * gridSize + chunkX];
            if (chunk.resolution < octreeResolution && chunk.loading != true) {
                dmThreat.pushWork(ChunkLoadInfo{gridCoord, octreeResolution});
                chunk.loading = true;
            }
        }
    }
}


#endif //CHUNK_MANAGEMENT_H
