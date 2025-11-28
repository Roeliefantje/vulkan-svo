//
// Created by roeld on 02/11/2025.
//

#include "chunk_generation_application.h"

#include <format>

#include "voxelizer.h"
#include "spdlog/spdlog.h"
namespace fs = std::filesystem;


ChunkGenerationApplication::ChunkGenerationApplication(Config config) : config(config) {
    if (!config.useHeightmapData) {
        objSceneMetaData = SceneMetadata(config.scene_path, config);
        std::cout << "ObjFile to be loaded: " << objSceneMetaData->objFile << std::endl;
        objFile = objSceneMetaData->objFile;
        config.voxelscale = 1.0f / objSceneMetaData->scale;
        config.scaleDistance = config.voxelscale / 0.00155f; //0.00155f is the size of a voxel for sub-pixel resolution at 1 meter with 80 fov.
        spdlog::info("Grid Height: {}", config.grid_height);
        spdlog::info("Voxel Scale: {}", config.voxelscale);
        spdlog::info("Scale Distance: {}", config.scaleDistance);
    } else {
        objFile = std::format("./assets/{}.obj", config.seed);
    }
    fs::path filePath{objFile};
    this->objDirectory = filePath.parent_path().string();
    this->directory = std::format("{}_{}", (filePath.parent_path() / filePath.stem()).string(), config.grid_size);

    glm::vec3 pos = config.useHeightmapData ? config.cameraPosition : config.cameraPosition * objSceneMetaData->scale;

    camera = CPUCamera(
        pos, config.cameraDirection,
        (int) config.width, (int) config.height,
        config.fov, config
    );

    if (!config.useHeightmapData) {
        float _scale;
        textures = std::map<std::string, LoadedTexture>();
        triangles = std::vector<TexturedTriangle>();
        int result = loadObject(objFile, objDirectory, config.chunk_resolution, config.grid_size, config.grid_height,
                                triangles.value(),
                                _scale);
    }
}

inline int positive_mod(const int a, const int b) {
    return (a % b + b) % b;
}

// Computes discrete LOD level based on distance
inline uint32_t computeLOD(float distance) {
    distance = std::max(distance, 1.0f); // avoid log2(0)
    return static_cast<uint32_t>(std::floor(std::log2(distance)));
}

inline uint32_t calculateChunkResolution(uint32_t maxChunkResolution, float distance) {
    uint32_t lod = computeLOD(distance);
    uint32_t resolution = maxChunkResolution >> lod; // divide by 2^lod
    return std::max(resolution, 8u); // clamp to some minimum
}

inline bool sceneInChunk(const Aabb &scene, const Aabb &chunk, const float &scale) {
    return (
        chunk.aa.x < (scene.bb.x * scale) &&
        chunk.bb.x >= (scene.aa.x * scale) &&
        chunk.aa.y < (scene.bb.y * scale) &&
        chunk.bb.y >= (scene.aa.y * scale) &&
        chunk.aa.z < (scene.bb.z * scale) &&
        chunk.bb.z >= (scene.aa.z * scale)
    );
}

void ChunkGenerationApplication::generateChunk(glm::ivec3 gridCoord, uint32_t resolution, glm::ivec3 chunkCoord) {
    uint32_t nodeAmount = 0;
    auto chunkFarValues = std::vector<uint32_t>();
    auto chunkOctreeGPU = std::vector<uint32_t>();
    if (!loadChunk(directory, config.chunk_resolution, resolution, chunkCoord, nodeAmount, chunkOctreeGPU,
                   chunkFarValues)) {
        // spdlog::debug("Chunk not yet created, generating the chunk");
        auto aabb = Aabb{};
        aabb.aa = glm::ivec3(chunkCoord.x * config.chunk_resolution, chunkCoord.y * config.chunk_resolution,
                             chunkCoord.z * config.chunk_resolution);
        aabb.bb = glm::ivec3(aabb.aa.x + config.chunk_resolution, aabb.aa.y + config.chunk_resolution,
                             aabb.aa.z + config.chunk_resolution);
        uint32_t maxDepth = std::ceil(std::log2(resolution));

        std::optional<OctreeNode> node = std::nullopt;
        if (config.useHeightmapData) {
            node = createChunkOctree(resolution, config.seed, chunkCoord, config.chunk_resolution, config.voxelscale,
                                     config.grid_height,
                                     nodeAmount);
        } else if (sceneInChunk(objSceneMetaData->sceneAabb, aabb, objSceneMetaData->scale)) {
            std::vector<uint32_t> allIndices(triangles.value().size());
            std::iota(allIndices.begin(), allIndices.end(), 0);
            node = createNode(aabb, triangles.value(), allIndices, textures.value(), nodeAmount, maxDepth, 0, objSceneMetaData.value());
        }


        if (node) {
            auto shared_node = std::make_shared<OctreeNode>(*node);
            addOctreeGPUdataBF(chunkOctreeGPU, shared_node, nodeAmount, chunkFarValues);
            if (!saveChunk(directory, config.chunk_resolution, resolution, chunkCoord, nodeAmount,
                           chunkOctreeGPU, chunkFarValues)) {
                std::cout << "Something went wrong storing Chunk data" << std::endl;
            }
        } else {
            saveChunk(directory, config.chunk_resolution, resolution, chunkCoord, nodeAmount,
                      chunkOctreeGPU, chunkFarValues);
        }
    }
}

void ChunkGenerationApplication::generateChunksForCameraPosition() {
    auto center = camera.gpu_camera.camera_grid_pos;
    glm::ivec3 start = center - int((camera.gridSize - 1) / 2);
    glm::ivec3 end = center + int((camera.gridSize - 1) / 2);
    spdlog::info("Generating {} Chunks!", camera.gridHeight * camera.gridSize * camera.gridSize);
    for (int chunkZ = 0; chunkZ < camera.gridHeight; chunkZ++) {
        for (int chunkY = start.y; chunkY < end.y; chunkY++) {
            for (int chunkX = start.x; chunkX < end.x; chunkX++) {
                auto gridCoord = glm::ivec3{
                    positive_mod(chunkX, camera.gridSize), positive_mod(chunkY, camera.gridSize), chunkZ
                };
                glm::ivec3 dist = glm::ivec3(chunkX, chunkY, chunkZ) - center;
                glm::vec3 worldDiff = glm::vec3(dist) * (
                                          static_cast<float>(config.chunk_resolution) * config.voxelscale);
                float distance = glm::length(worldDiff) - (
                                     static_cast<float>(config.chunk_resolution) * config.voxelscale);
                uint32_t octreeResolution = calculateChunkResolution(config.chunk_resolution, distance / config.scaleDistance);

                //The cpu chunks location, so not the buffer location we store it in, which is gridCoord, but the coords of the chunk itself.
                glm::ivec3 chunkCoord = camera.chunk_coords + dist;
                generateChunk(gridCoord, octreeResolution, chunkCoord);
            }
        }
    }
}

void ChunkGenerationApplication::genererateChunks() {
    if (config.cameraKeyFrames) {
        std::vector<std::string> computed_chunks = {};
        const auto start = config.cameraKeyFrames.value().front().time;
        const auto end = config.cameraKeyFrames.value().back().time;
        constexpr float timeBetweenFrames = (1.0 / 60.0);
        for (float t = start; t < (end + timeBetweenFrames); t += timeBetweenFrames) {
            auto kf = interpolateCamera(config.cameraKeyFrames.value(), t);
            if (!config.useHeightmapData) {
                camera.setPosition(kf.position * objSceneMetaData->scale);
            } else {
                camera.setPosition(kf.position);
            }
            const std::string key = std::format("{}, {}, {}", camera.chunk_coords.x, camera.chunk_coords.y, camera.chunk_coords.z);
            bool already_computed = false;
            for (const auto &computed_chunk : computed_chunks)
            {
                if (computed_chunk == key)
                {
                    already_computed = true;
                    break;
                }
            }
            if (already_computed) {continue;}
            computed_chunks.push_back(key);
            spdlog::info("Generating Chunks around Camera chunk: {}", key);
            camera.gpu_camera.direction = glm::normalize(kf.direction);
            generateChunksForCameraPosition();
        }
    } else {
        generateChunksForCameraPosition();
    }
}
