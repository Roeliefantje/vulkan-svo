//
// Created by roeld on 02/11/2025.
//

#include "chunk_generation_application.h"

#include <format>

#include "voxelizer.h"
namespace fs = std::filesystem;


ChunkGenerationApplication::ChunkGenerationApplication(Config config) : config(config) {
    if (!config.useHeightmapData) {
        objSceneMetaData = SceneMetadata("./assets/san-miguel-low-poly.obj", config);
        std::cout << "ObjFile to be loaded: " << objSceneMetaData->objFile << std::endl;
        objFile = objSceneMetaData->objFile;
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
        int totalResolution = config.chunk_resolution * config.grid_size;
        float _scale;
        textures = std::map<std::string, LoadedTexture>();
        triangles = std::vector<TexturedTriangle>();
        int result = loadObject(objFile, objDirectory, totalResolution, config.grid_size, triangles.value(), _scale);
    }
}

inline int positive_mod(int a, int b) {
    return (a % b + b) % b;
}

inline uint32_t calculateChunkResolution(uint32_t maxChunkResolution, int dist) {
    return maxChunkResolution >> (dist);
}

void ChunkGenerationApplication::generateChunk(glm::ivec2 gridCoord, uint32_t resolution, glm::ivec2 chunkCoord) {
    uint32_t nodeAmount = 0;
    auto chunkFarValues = std::vector<uint32_t>();
    auto chunkOctreeGPU = std::vector<uint32_t>();
    if (!loadChunk(directory, config.chunk_resolution, resolution, chunkCoord, nodeAmount, chunkOctreeGPU,
                   chunkFarValues)) {
        std::cout << "Chunk not yet created, generating the chunk" << std::endl;
        auto aabb = Aabb{};
        aabb.aa = glm::ivec3(chunkCoord.x * config.chunk_resolution, chunkCoord.y * config.chunk_resolution, 0);
        aabb.bb = glm::ivec3(aabb.aa.x + config.chunk_resolution, aabb.aa.y + config.chunk_resolution,
                             config.chunk_resolution);
        uint32_t maxDepth = std::ceil(std::log2(resolution));

        std::optional<OctreeNode> node = std::nullopt;
        if (config.useHeightmapData) {
            std::cout << "Get in here as expected" << std::endl;
            uint32_t scale = config.chunk_resolution / resolution;
            node = createChunkOctree(resolution, config.seed, chunkCoord, scale, nodeAmount);
        } else {
            std::vector<uint32_t> allIndices(triangles.value().size());
            std::iota(allIndices.begin(), allIndices.end(), 0);
            node = createNode(aabb, triangles.value(), allIndices, textures.value(), nodeAmount, maxDepth, 0);
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

void ChunkGenerationApplication::genererateChunks() {
    auto center = camera.gpu_camera.camera_grid_pos;
    glm::ivec3 start = center - int(camera.gridSize / 2);
    for (int chunkY = start.y; chunkY < camera.gridSize; chunkY++) {
        for (int chunkX = start.x; chunkX < camera.gridSize; chunkX++) {
            auto gridCoord = glm::ivec2{positive_mod(chunkX, camera.gridSize), positive_mod(chunkY, camera.gridSize)};
            glm::ivec3 dist = glm::ivec3(chunkX, chunkY, center.z) - center;
            int single_axis_dist = std::max(abs(chunkY - center.y), abs(chunkX - center.x));
            uint32_t octreeResolution = calculateChunkResolution(config.chunk_resolution, single_axis_dist);

            //The cpu chunks location, so not the buffer location we store it in, which is gridCoord, but the coords of the chunk itself.
            glm::ivec3 chunkCoord = camera.chunk_coords + dist;
            //As we are dealing with a 2d grid on the gpu side, we do not deal with chunkCoord z distance and stuffs.
            chunkCoord.z = 0;
            generateChunk(gridCoord, octreeResolution, chunkCoord);
        }
    }
}
