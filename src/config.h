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
    bool useHeightmapData = true;
    float voxelscale = 0.0155f;
    float scaleDistance = 10.0f; //At what distance would the voxelScale be equivalent to a pixel? TODO: calculate this automagically

    //Output screen size
    uint32_t width = 1920;
    uint32_t height = 1080;

    //What value are we using for the gpu compute groups
    float x_groupsize = 16;
    float y_groupsize = 16;
    size_t GIGABYTE = (1 << 30);

    VkDeviceSize staging_size = GIGABYTE << 1;
    uint32_t chunk_resolution = 1024;
    uint32_t grid_size = 31;
    uint32_t grid_height = useHeightmapData
                               ? std::min(40u, static_cast<uint32_t>(std::ceil(
                                              400 / (static_cast<float>(chunk_resolution) * voxelscale))))
                               : 5;
    uint32_t seed = 12345 * 6;
    std::optional<std::vector<CameraKeyFrame> > cameraKeyFrames;

    //MouseSensitivity
    float mouseSensitivity = 0.01f;
    //The default camera positions don't really get used, its rather just a fallback.
    glm::vec3 cameraPosition = glm::vec3(0, 0, 712.5);
    glm::vec3 cameraDirection = glm::vec3(0.5, 0.5, 0);
    float fov = 0.52359 * 3; //30 degrees in radians
    std::string scene_path = "./assets/San_Miguel/san-miguel-low-poly.obj";
    std::string camera_path = "./camera-hm.json";
    std::string camera_keyframe_path = "./camera_path.json";

    bool chunkgen = false;
    bool allowUserInput = true;
    bool printChunkDebug = false;
    spdlog::level::level_enum loglevel = spdlog::level::debug;

    Config(int argc, char *argv[]);

    void read_keyframes();

    void generate_keyframes(float distance, float height);
};

CameraKeyFrame interpolateCamera(const std::vector<CameraKeyFrame> &keyframes, const float currentTime);

#endif //CONFIG_H
