#pragma once

#include <filesystem>
#include <fstream>
#include <iostream>
#include <format>


#include "structures.h"

#ifndef CHUNK_MANAGEMENT_H
#define CHUNK_MANAGEMENT_H

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

bool saveObj(const std::string &inputFile, const std::string &path, uint32_t svo_resolution, uint32_t &nodeCount,
             std::vector<uint32_t> &gpuData, std::vector<uint32_t> &farValues) {
    try {
        namespace fs = std::filesystem;

        fs::path dirPath(path);
        fs::path inputPath = dirPath / inputFile;
        std::string fileName = inputPath.stem().string() + "_res" + std::to_string(svo_resolution) + ".svo";
        fs::path outFilePath = inputPath.parent_path() / fileName;

        std::ofstream outFile(outFilePath, std::ios::binary);
        if (!outFile) return false;

        // Write metadata
        // outFile.write(reinterpret_cast<char *>(&svo_resolution), sizeof(svo_resolution));
        outFile.write(reinterpret_cast<char *>(&nodeCount), sizeof(nodeCount));

        // Write vector sizes
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


bool loadObj(const std::string &inputFile, const std::string &path, uint32_t svo_resolution, uint32_t &nodeCount,
             std::vector<uint32_t> &gpuData, std::vector<uint32_t> &farValues) {
    try {
        namespace fs = std::filesystem;

        fs::path dirPath(path);
        fs::path inputPath = dirPath / inputFile;
        std::string fileName = inputPath.stem().string() + "_res" + std::to_string(svo_resolution) + ".svo";
        fs::path outFilePath = inputPath.parent_path() / fileName;
        std::ifstream inFile(outFilePath, std::ios::binary);
        if (!inFile) return false;

        // Read metadata
        // inFile.read(reinterpret_cast<char *>(&svo_resolution), sizeof(svo_resolution));
        inFile.read(reinterpret_cast<char *>(&nodeCount), sizeof(nodeCount));

        // Read vector sizes
        uint32_t gpuDataSize, farValuesSize;
        inFile.read(reinterpret_cast<char *>(&gpuDataSize), sizeof(gpuDataSize));
        inFile.read(reinterpret_cast<char *>(&farValuesSize), sizeof(farValuesSize));

        // Read vectors
        size_t old_gpu_data_size = gpuData.size();
        size_t old_far_values_size = farValues.size();

        gpuData.resize(gpuDataSize + old_gpu_data_size);
        farValues.resize(farValuesSize + old_far_values_size);
        inFile.read(reinterpret_cast<char *>(gpuData.data() + old_gpu_data_size), gpuDataSize * sizeof(uint32_t));
        inFile.read(reinterpret_cast<char *>(farValues.data() + old_far_values_size), farValuesSize * sizeof(uint32_t));

        inFile.close();
        return true;
    } catch (...) {
        return false;
    }
}


#endif //CHUNK_MANAGEMENT_H
