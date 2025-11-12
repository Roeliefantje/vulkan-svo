//
// Created by roeld on 01/10/2025.
//
#pragma once

#ifndef CONFIG_H
#define CONFIG_H
#include <cxxopts.hpp>
#include <fstream>
#include <iostream>
#include <glm/vec3.hpp>
#include <nlohmann/json.hpp>
#include <vulkan/vulkan_core.h>

#include "spdlog/common.h"

struct CameraKeyFrame {
    float time; //Seconds since start
    glm::vec3 position;
    glm::vec3 direction;
};

struct Config {
    //Whether we use the voxelizer or the heightmap data.
    bool useHeightmapData = false;

    //Output screen size
    uint32_t width = 1920;
    uint32_t height = 1080;

    //What value are we using for the gpu compute groups
    float x_groupsize = 16;
    float y_groupsize = 16;
    size_t GIGABYTE = (1 << 30);

    VkDeviceSize staging_size = GIGABYTE << 1;
    uint32_t chunk_resolution = 1024;
    uint32_t grid_size = 25;
    uint32_t grid_height = 5;
    uint32_t seed = 12345 * 6;
    std::optional<std::vector<CameraKeyFrame> > cameraKeyFrames;

    //MouseSensitivity
    float mouseSensitivity = 0.01f;
    //The default camera positions dont really get used, its rather just a fallback.
    glm::vec3 cameraPosition = glm::vec3(100 + chunk_resolution, 100 + chunk_resolution, 712.5);
    glm::vec3 cameraDirection = glm::vec3(0.5, 0.5, 0);
    float fov = 0.52359; //30 degrees in radians
    std::string scene_path = "./assets/san-miguel-low-poly.obj";
    std::string camera_path = "./camera.json";
    std::string camera_keyframe_path = "./camera_path.json";

    bool chunkgen = false;
    bool allowUserInput = true;
    bool printChunkDebug = true;
    spdlog::level::level_enum loglevel = spdlog::level::debug;

    Config(int argc, char *argv[]);

    void read_keyframes();
};

CameraKeyFrame interpolateCamera(const std::vector<CameraKeyFrame> &keyframes, const float currentTime);

#endif //CONFIG_H
