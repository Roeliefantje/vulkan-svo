#pragma once


#ifndef CHUNK_MANAGEMENT_H
#define CHUNK_MANAGEMENT_H
#include <filesystem>
#include <fstream>
#include <iostream>
#include <format>


// #include "data_manage_threat.h"
#include "structures.h"

bool saveChunk(const std::string &scenePath, uint32_t max_resolution, uint32_t svo_resolution, glm::ivec3 gridCoords,
               uint32_t &nodeCount,
               std::vector<uint32_t> &gpuData, std::vector<uint32_t> &farValues);

bool loadChunk(const std::string &scenePath, uint32_t max_resolution, uint32_t svo_resolution, glm::ivec3 gridCoords,
               uint32_t &nodeCount,
               std::vector<uint32_t> &gpuData, std::vector<uint32_t> &farValues);


#endif //CHUNK_MANAGEMENT_H
