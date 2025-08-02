//
// Created by roeld on 02/08/2025.
//
#pragma once
#define TINYOBJLOADER_IMPLEMENTATION
#include <iostream>

#include "structures.h"
#include "tiny_obj_loader.h"
#include "tribox.h"

#ifndef VOXELIZER_H
#define VOXELIZER_H

struct TexturedTriangle {
    glm::vec3 v[3];
    glm::vec2 uv[3];
    std::string diffuse_texname; // empty if none
};


int loadObject(std::string inputFile, std::string path, int resolution,
               std::vector<std::shared_ptr<TexturedTriangle> > &triangles) {
    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = path;

    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(inputFile, reader_config)) {
        if (!reader.Error().empty()) {
            std::cerr << "TinyObjReader: " << reader.Error();
        }
        return 1;
    }

    if (!reader.Warning().empty()) {
        std::cout << "TinyObjReader: " << reader.Warning();
    }

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &materials = reader.GetMaterials();

    glm::vec3 bbMin{
        +std::numeric_limits<float>::infinity(),
        +std::numeric_limits<float>::infinity(),
        +std::numeric_limits<float>::infinity()
    };
    glm::vec3 bbMax{
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity(),
        -std::numeric_limits<float>::infinity()
    };

    std::cout << "Amount of Vertices: " << attrib.vertices.size() << std::endl;
    triangles.reserve(attrib.vertices.size() / 9);


    for (size_t v = 0; v < attrib.vertices.size() / 3; v++) {
        float x = attrib.vertices[3 * v + 0];
        float y = attrib.vertices[3 * v + 2];
        float z = attrib.vertices[3 * v + 1];
        bbMin.x = std::min(bbMin.x, x);
        bbMax.x = std::max(bbMax.x, x);
        bbMin.y = std::min(bbMin.y, y);
        bbMax.y = std::max(bbMax.y, y);
        bbMin.z = std::min(bbMin.z, z);
        bbMax.z = std::max(bbMax.z, z);
    }

    std::cout << "Scene AABB:\n"
            << "  Min = (" << bbMin.x << ", " << bbMin.y << ", " << bbMin.z << ")\n"
            << "  Max = (" << bbMax.x << ", " << bbMax.y << ", " << bbMax.z << ")"
            << std::endl;
    glm::vec3 sceneSize = {bbMax.x - bbMin.x, bbMax.y - bbMin.y, bbMax.z - bbMin.z};
    glm::vec3 offset = bbMin;
    float scale = resolution / std::max({sceneSize.x, sceneSize.y, sceneSize.z});

    // Loop over shapes
    for (size_t s = 0; s < shapes.size(); s++) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++) {
            size_t fv = size_t(shapes[s].mesh.num_face_vertices[f]);
            if (fv != 3) {
                std::cerr << "Non-triangle face found; skipping\n";
                index_offset += fv;
                continue;
            }
            TexturedTriangle tri;
            int matID = shapes[s].mesh.material_ids[f]; // -1 if no material
            if (matID >= 0 && matID < (int) materials.size()) {
                tri.diffuse_texname = materials[matID].diffuse_texname;
            }

            // For each of the 3 vertices:
            for (int v = 0; v < 3; ++v) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                // Fetch Position and scale them to the resolution.
                tri.v[v].x = (attrib.vertices[3 * idx.vertex_index + 0] - offset.x) * scale;
                tri.v[v].y = (attrib.vertices[3 * idx.vertex_index + 2] - offset.y) * scale;
                tri.v[v].z = (attrib.vertices[3 * idx.vertex_index + 1] - offset.z) * scale;

                // Texcoord (might be missing)
                if (idx.texcoord_index >= 0) {
                    tri.uv[v].x = attrib.texcoords[2 * idx.texcoord_index + 0];
                    tri.uv[v].y = attrib.texcoords[2 * idx.texcoord_index + 1];
                } else {
                    tri.uv[v].x = tri.uv[v].y = 0.0f;
                }
            }

            triangles.push_back(std::make_shared<TexturedTriangle>(tri));
            index_offset += fv;
        }
    }

    triangles.shrink_to_fit();
    return 0;
}

std::optional<OctreeNode> createNode(Aabb aabb, std::vector<std::shared_ptr<TexturedTriangle> > &parentTriangles,
                                     uint32_t &nodeCount, uint32_t &maxDepth, uint32_t currentDepth) {
    auto node = OctreeNode();

    std::vector<std::shared_ptr<TexturedTriangle> > triangles;
    glm::vec3 midpoint = glm::vec3(aabb.aa + aabb.bb) / 2.0f;
    glm::vec3 halfSize = glm::vec3(aabb.bb - aabb.aa) / 2.0f;

    float mp[3] = {float(midpoint.x), float(midpoint.y), float(midpoint.z)};
    float hs[3] = {float(halfSize.x), float(halfSize.y), float(halfSize.z)};

    for (const auto &triPtr: parentTriangles) {
        if (triPtr) {
            // Access members via pointer
            auto v0 = triPtr->v[0];
            auto v1 = triPtr->v[1];
            auto v2 = triPtr->v[2];
            // auto tex = triPtr->texture;
            // do something...
            float triverts[3][3] = {
                {v0.x, v0.y, v0.z},
                {v1.x, v1.y, v1.z},
                {v2.x, v2.y, v2.z}
            };

            if (triBoxOverlap(mp, hs, triverts)) {
                triangles.push_back(triPtr);
            }
        }
    }

    if (triangles.size() == 0) {
        return std::nullopt;
    }

    if (currentDepth < maxDepth) {
        //Check Children
        for (int z = 0; z < 2; z++) {
            for (int y = 0; y < 2; y++) {
                for (int x = 0; x < 2; x++) {
                    auto childaabb = Aabb{};
                    childaabb.aa = glm::ivec3{
                        x == 0 ? aabb.aa.x : midpoint.x,
                        y == 0 ? aabb.aa.y : midpoint.y,
                        z == 0 ? aabb.aa.z : midpoint.z
                    };
                    childaabb.bb = glm::ivec3{
                        x == 0 ? midpoint.x : aabb.bb.x,
                        y == 0 ? midpoint.y : aabb.bb.y,
                        z == 0 ? midpoint.z : aabb.bb.z
                    };


                    auto child = createNode(childaabb, triangles, nodeCount, maxDepth, currentDepth + 1);
                    if (child) {
                        int childIndex = z * 4 + y * 2 + x;
                        node.childMask |= 1u << (7 - childIndex);
                        node.children[childIndex] = std::make_shared<OctreeNode>(*child);
                    }
                }
            }
        }
    } else {
        //Do color stuff
        node.color = getColor(aabb.bb.z, aabb.bb.x, aabb.bb.y);
    }

    nodeCount++;
    return node;
}


OctreeNode voxelizeObj(std::string inputFile, std::string path, uint32_t svo_resolution, uint32_t &nodeCount) {
    std::vector<std::shared_ptr<TexturedTriangle> > triangles;
    int result = loadObject(inputFile, path, svo_resolution, triangles);

    auto aabb = Aabb{};
    aabb.aa = glm::ivec3(0, 0, 0);
    aabb.bb = glm::ivec3(svo_resolution, svo_resolution, svo_resolution);
    uint32_t maxDepth = std::ceil(std::log2(svo_resolution));
    std::cout << "Creating nodes" << std::endl;
    auto node = createNode(aabb, triangles, nodeCount, maxDepth, 0);
    std::cout << "Finished creating nodes " << triangles.size() << std::endl;

    if (node) {
        return *node;
    }

    return OctreeNode();
}


#endif //VOXELIZER_H
