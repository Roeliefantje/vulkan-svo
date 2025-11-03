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
    ChunkGenerationApplication(Config config);

    void generateChunk(glm::ivec2 gridCoord, uint32_t resolution, glm::ivec2 chunkCoord);

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
