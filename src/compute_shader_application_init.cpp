//
// Created by roeld on 23/10/2025.
//
#include "compute_shader_application.h"

ComputeShaderApplication::ComputeShaderApplication(Config config) : config(config) {
    initData();
    initCamera();

    initWindow();
    initVulkan();
    initThreads();
}

void ComputeShaderApplication::initData() {
    // This function initializes all the vectors that get copied over onto the GPU,
    // as we are loading in everything after the initial setup through the staging buffer,
    // we actually dont do anything other than setting the size of those buffers here.
    gridValues = std::vector<Chunk>(config.grid_size * config.grid_size);
    cpuGridValues = std::vector<CpuChunk>(config.grid_size * config.grid_size);
    farValues = std::vector<uint32_t>(config.GIGABYTE / sizeof(uint32_t));
    octreeGPU = std::vector<uint32_t>(config.GIGABYTE * 3 / sizeof(uint32_t));

    //GPU Ubo object? I think...
    gridInfo = GridInfo{config.chunk_resolution, config.grid_size};
}

void ComputeShaderApplication::initCamera() {
    // Initialize the camera position based on the config information.
    //TODO!: If the config includes a json file for the camera position, use it.
    if (config.useHeightmapData) {
        std::cout << "Initializing Camera" << std::endl;
        camera = Camera{
            glm::vec3(10, 10, 712.5), glm::vec3(20, 20, 710.5), (int) config.width, (int) config.height,
            glm::radians(30.0f)
        };
    } else {
        throw std::runtime_error("Camera initialization not yet implemented with scene!");
    }
}
