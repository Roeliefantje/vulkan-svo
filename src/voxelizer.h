//
// Created by roeld on 02/08/2025.
//
#pragma once
#include <filesystem>
#include <fstream>

#define TINYOBJLOADER_IMPLEMENTATION
#include <iostream>

#ifndef VOXELIZER_H
#define VOXELIZER_H

#include <map>

#include "svo_generation.h"
#include "structures.h"
#include "chunk_management.h"
#include "scene_metadata.h"


//TODO: Improve memory usage by only storing the triangles that are inside of a chunk
int loadSceneMetaData(std::string inputFile, std::string path, Aabb &sceneBounds, int &numTriangles);

int loadObject(std::string inputFile, std::string path, int resolution, int gridSize, int gridHeight,
               std::vector<TexturedTriangle> &triangles, float &scale);


LoadedTexture &loadImage(const std::string &tex_name, std::map<std::string, LoadedTexture> &loadedTextures);

glm::vec2 getTextureUV(glm::vec3 &midpoint, const TexturedTriangle &triangle);

std::optional<OctreeNode> createNode(Aabb aabb, std::vector<TexturedTriangle> &globalTriangles,
                                     std::vector<uint32_t> &parentTriIndices,
                                     std::map<std::string, LoadedTexture> &loadedTextures, uint32_t &nodeCount,
                                     uint32_t &maxDepth, uint32_t currentDepth, SceneMetadata &metadata);

#endif //VOXELIZER_H
