//
// Created by roeld on 23/10/2025.
//

#include "scene_metadata.h"
#include <fstream>
#include <nlohmann/json.hpp>
#include "voxelizer.h"

using json = nlohmann::json;
namespace fs = std::filesystem;


SceneMetadata::SceneMetadata(std::string objFile, Config &config) : objFile(objFile) {
    loadMetaData(config);
}

void SceneMetadata::loadMetaData(Config &config) {
    fs::path filePath{objFile};
    fs::path jsonPath = filePath.replace_extension(".json");
    if (!fs::exists(jsonPath)) {
        std::cout << "Json metadata file does not exist yet, loading scene for meta data." << std::endl;
        auto directory = filePath.parent_path().string();;
        loadSceneMetaData(objFile, directory, sceneAabb, numTriangles);
        saveMetaData();
    } else {
        std::cout << "Loading Scene Meta Data from json" << std::endl;
        std::ifstream f(jsonPath.string());
        json data = json::parse(f);
        sceneAabb.aa.x = data["aabb"]["aa"]["x"];
        sceneAabb.aa.y = data["aabb"]["aa"]["y"];
        sceneAabb.aa.z = data["aabb"]["aa"]["z"];
        sceneAabb.bb.x = data["aabb"]["bb"]["x"];
        sceneAabb.bb.y = data["aabb"]["bb"]["y"];
        sceneAabb.bb.z = data["aabb"]["bb"]["z"];
        numTriangles = data["numTriangles"];
    }


    auto maxChunkResolution = config.chunk_resolution / config.grid_size;
    scale = std::min(
        (config.chunk_resolution * config.grid_size) / std::max(float(sceneAabb.bb.x), float(sceneAabb.bb.y)),
        config.chunk_resolution / (float) sceneAabb.bb.z
    );
}

void SceneMetadata::saveMetaData() {
    json data;
    data["aabb"]["aa"]["x"] = sceneAabb.aa.x;
    data["aabb"]["aa"]["y"] = sceneAabb.aa.y;
    data["aabb"]["aa"]["z"] = sceneAabb.aa.z;
    data["aabb"]["bb"]["x"] = sceneAabb.bb.x;
    data["aabb"]["bb"]["y"] = sceneAabb.bb.y;
    data["aabb"]["bb"]["z"] = sceneAabb.bb.z;
    data["numTriangles"] = numTriangles;

    fs::path filePath{objFile};
    fs::path jsonPath = filePath.replace_extension(".json");
    std::ofstream file(jsonPath.string(), std::ios::trunc);
    file << std::setw(4) << data << std::endl;
}
