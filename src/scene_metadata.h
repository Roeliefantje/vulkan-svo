//
// Created by roeld on 23/10/2025.
//

#ifndef OBJ_METADATA_H
#define OBJ_METADATA_H
#include <string>
#include <glm/fwd.hpp>

#include "svo_generation.h"


struct SceneMetadata {
    std::string objFile = "";
    Aabb sceneAabb;
    int numTriangles = 0;
    float scale = 0;

    SceneMetadata() = default;

    SceneMetadata(std::string objFile, Config &config);

    void loadMetaData(Config &config);

    void saveMetaData();
};


#endif //OBJ_METADATA_H
