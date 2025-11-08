//
// Created by roeld on 02/11/2025.
//

#ifndef CHUNK_GENERATION_APPLICATION_H
#define CHUNK_GENERATION_APPLICATION_H
#include "config.h"
#include "scene_metadata.h"
#include "structures.h"


class ChunkGenerationApplication {
public:
    explicit ChunkGenerationApplication(Config config);

    void generateChunk(glm::ivec3 gridCoord, uint32_t resolution, glm::ivec3 chunkCoord);

    void generateChunksForCameraPosition();

    void genererateChunks();


    Config config;
    CPUCamera camera;
    std::optional<SceneMetadata> objSceneMetaData;
    std::optional<std::vector<TexturedTriangle> > triangles = std::nullopt;
    std::optional<std::map<std::string, LoadedTexture> > textures = std::nullopt;
    std::string objFile;
    std::string objDirectory;
    std::string directory;
};


#endif //CHUNK_GENERATION_APPLICATION_H
