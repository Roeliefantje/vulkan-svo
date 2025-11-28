//
// Created by roeld on 23/10/2025.
//
#include "compute_shader_application.h"
#include "spdlog/spdlog.h"

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
    if (!config.useHeightmapData) {
        objSceneMetaData = SceneMetadata(config.scene_path, config);
        spdlog::debug("ObjFile to be loaded: {}", objSceneMetaData->objFile);
        config.voxelscale = 1.0f / objSceneMetaData->scale;
        config.scaleDistance = config.voxelscale / 0.00155f;
        spdlog::info("Grid Height: {}", config.grid_height);
        spdlog::info("Voxel Scale: {}", config.voxelscale);
        spdlog::info("Scale Distance: {}", config.scaleDistance);
    }

    gridValues = std::vector<Chunk>(config.grid_height * config.grid_size * config.grid_size);
    cpuGridValues = std::vector<CpuChunk>(config.grid_height * config.grid_size * config.grid_size);
    farValues = std::vector<uint32_t>(config.GIGABYTE / sizeof(uint32_t));
    octreeGPU = std::vector<uint32_t>(config.GIGABYTE * 3 / sizeof(uint32_t));

    //GPU Ubo object? I think...
    gridInfo = GridInfo(config.chunk_resolution, config.grid_size, config.grid_height);
}


void ComputeShaderApplication::initCamera() {
    // Initialize the camera position based on the config information.
    glm::vec3 pos = config.useHeightmapData ? config.cameraPosition : config.cameraPosition * objSceneMetaData->scale;

    camera = CPUCamera(
        pos, config.cameraDirection,
        (int) config.width, (int) config.height,
        config.fov, config
    );

    spdlog::debug("Finished setting up Camera");
}

void ComputeShaderApplication::initThreads() {
    //Init the data and threads necessary for gpu chunk stuffs.
    farValuesGPUManager = new BufferManager(farValuesSBuffers[0], farValues.size(),
                                            "Far Values Buffer", sizeof(uint32_t));
    octreeGPUManager = new BufferManager(shaderStorageBuffers[0][0], octreeGPU.size(),
                                         "Octree Nodes Buffer", sizeof(uint32_t));
    stagingBufferManager = new BufferManager(shaderStorageBuffers[0][0], config.staging_size,
                                             "Staging Buffer", 1);


    dmThreat = new DataManageThreat(device, stagingBufferProperties, config, *octreeGPUManager, *stagingBufferManager,
                                    gridBuffers[0], *farValuesGPUManager, objSceneMetaData, cpuGridValues, camera);
}
