//
// Created by roeld on 22/10/2025.
//
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "voxelizer.h"

#include <algorithm>

#include "tiny_obj_loader.h"
#include "tribox.h"

int loadSceneMetaData(std::string inputFile, std::string path, Aabb &sceneBounds, int &numTriangles) {
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
    numTriangles = shapes.size();

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
    sceneBounds.aa = bbMin - bbMin;
    sceneBounds.bb = bbMax - bbMin;

    return 0;
}


int loadObject(std::string inputFile, std::string path, int resolution, int gridSize,
               std::vector<TexturedTriangle> &triangles, float &scale) {
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
    auto maxChunkResolution = resolution / gridSize;
    scale = std::min(
        (resolution) / std::max(sceneSize.x, sceneSize.y),
        (float) maxChunkResolution / sceneSize.z
    );
    // float scale = resolution / std::max({sceneSize.x, sceneSize.y, sceneSize.z});
    // float scaleZ = scale / gridSize; //Compensate for the grid

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
                tri.diffuse = glm::vec3(materials[matID].diffuse[0], materials[matID].diffuse[1],
                                        materials[matID].diffuse[2]);
            } else {
                tri.diffuse = glm::vec3(0.8f);
            }

            // For each of the 3 vertices:
            for (int v = 0; v < 3; ++v) {
                tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

                // Fetch Position and scale them to the resolution.
                tri.v[v].x = (attrib.vertices[3 * idx.vertex_index + 0] - offset.x) * scale;
                tri.v[v].y = (attrib.vertices[3 * idx.vertex_index + 2] - offset.y) * scale;
                tri.v[v].z = (attrib.vertices[3 * idx.vertex_index + 1] - offset.z) * scale;

                if (tri.v[v].z > (resolution / gridSize)) {
                    std::cout << "Z is wrong!" << std::endl;
                }

                // Texcoord (might be missing)
                if (idx.texcoord_index >= 0) {
                    tri.uv[v].x = attrib.texcoords[2 * idx.texcoord_index + 0];
                    tri.uv[v].y = attrib.texcoords[2 * idx.texcoord_index + 1];
                } else {
                    tri.uv[v].x = tri.uv[v].y = 0.0f;
                }
            }

            triangles.push_back(tri);
            index_offset += fv;
        }
    }

    triangles.shrink_to_fit();
    return 0;
}

int loadObject(std::string inputFile, std::string path, int resolution, int gridSize,
               std::vector<TexturedTriangle> &triangles, float &scale);


LoadedTexture &loadImage(const std::string &tex_name, std::map<std::string, LoadedTexture> &loadedTextures) {
    if (loadedTextures.contains(tex_name)) {
        return loadedTextures[tex_name];
    }

    const std::string sub_folder = "./assets/";
    int texWidth, texHeight, channels;
    unsigned char *imageData = stbi_load((sub_folder + tex_name).c_str(), &texWidth, &texHeight, &channels, 3);

    loadedTextures[tex_name] = LoadedTexture{imageData, texWidth, texHeight, channels};;
    return loadedTextures[tex_name];
}

glm::vec2 getTextureUV(glm::vec3 &midpoint, const TexturedTriangle &triangle) {
    auto v0 = triangle.v[1] - triangle.v[0];
    auto v1 = triangle.v[2] - triangle.v[0];
    auto v2 = midpoint - triangle.v[0];

    float d00 = dot(v0, v0);
    float d01 = dot(v0, v1);
    float d11 = dot(v1, v1);
    float d20 = dot(v2, v0);
    float d21 = dot(v2, v1);

    float denom = d00 * d11 - d01 * d01;
    float v = (d11 * d20 - d01 * d21) / denom;
    float w = (d00 * d21 - d01 * d20) / denom;
    float u = 1.0f - v - w;

    return triangle.uv[0] * u + triangle.uv[1] * v + triangle.uv[2] * w;
}

std::optional<OctreeNode> createNode(Aabb aabb, std::vector<TexturedTriangle> &globalTriangles,
                                     std::vector<uint32_t> &parentTriIndices,
                                     std::map<std::string, LoadedTexture> &loadedTextures, uint32_t &nodeCount,
                                     uint32_t &maxDepth, uint32_t currentDepth) {
    auto node = OctreeNode();

    // std::vector<std::shared_ptr<TexturedTriangle> > triangles;
    std::vector<uint32_t> triangleIndices;
    glm::vec3 midpoint = glm::vec3(aabb.aa + aabb.bb) / 2.0f;
    glm::vec3 halfSize = glm::vec3(aabb.bb - aabb.aa) / 2.0f;

    float mp[3] = {float(midpoint.x), float(midpoint.y), float(midpoint.z)};
    float hs[3] = {float(halfSize.x), float(halfSize.y), float(halfSize.z)};

    for (uint32_t triIdx: parentTriIndices) {
        const auto &tri = globalTriangles[triIdx];
        float triverts[3][3] = {
            {tri.v[0].x, tri.v[0].y, tri.v[0].z},
            {tri.v[1].x, tri.v[1].y, tri.v[1].z},
            {tri.v[2].x, tri.v[2].y, tri.v[2].z}
        };

        if (triBoxOverlap(mp, hs, triverts)) {
            triangleIndices.push_back(triIdx);
        }
    }

    if (triangleIndices.size() == 0) {
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


                    auto child = createNode(childaabb, globalTriangles, triangleIndices, loadedTextures, nodeCount,
                                            maxDepth,
                                            currentDepth + 1);
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
        const auto &tri = globalTriangles[triangleIndices[0]];
        auto texture = loadImage(tri.diffuse_texname, loadedTextures);
        if (!texture.imageData) {
            uint8_t r = tri.diffuse.r * 255;
            uint8_t g = tri.diffuse.g * 255;
            uint8_t b = tri.diffuse.b * 255;
            node.color = (r << 16) | (g << 8) | b;
        } else {
            glm::vec2 uv = glm::mod(getTextureUV(midpoint, tri), glm::vec2(1.0f));

            int px = static_cast<int>(uv.x * texture.texWidth);
            int py = static_cast<int>((1.0f - uv.y) * texture.texHeight); // flip Y axis

            px = std::clamp(px, 0, texture.texWidth - 1);
            py = std::clamp(py, 0, texture.texHeight - 1);
            // // triangles[0]->uv[0]
            int pixelIndex = (py * texture.texWidth + px) * 3;
            uint8_t r = texture.imageData[pixelIndex + 0];
            uint8_t g = texture.imageData[pixelIndex + 1];
            uint8_t b = texture.imageData[pixelIndex + 2];

            node.color = (r << 16) | (g << 8) | b;
        }
    }

    nodeCount++;
    return node;
}

int fetchresolution(uint32_t maxChunkResolution, float distance) {
    int lod_reduction = 0;

    if (distance > 10.0f) lod_reduction++;
    if (distance > 20.0f) lod_reduction++;
    if (distance > 30.0f) lod_reduction++;

    return std::max(maxChunkResolution >> lod_reduction, 64u);
}

//TODO: Rewrite to save and load chunks
void gridVoxelizeScene(std::vector<Chunk> &gridValues, std::vector<uint32_t> &farValues,
                       std::vector<uint32_t> &octreeGPU, CPUCamera &camera,
                       uint32_t maxChunkResolution, uint32_t gridSize, std::string inputFile, std::string path,
                       glm::vec3 cameraPosition, glm::vec3 cameraLookAt,
                       uint32_t screenWidth, uint32_t screenHeight, Config &config) {
    gridValues = std::vector<Chunk>(gridSize * gridSize);
    farValues = std::vector<uint32_t>();
    octreeGPU = std::vector<uint32_t>();

    std::vector<TexturedTriangle> triangles;
    int totalResolution = maxChunkResolution * gridSize;
    float scale;

    int result = loadObject(inputFile, path, totalResolution, gridSize, triangles, scale);

    std::vector<uint32_t> allIndices(triangles.size());
    std::iota(allIndices.begin(), allIndices.end(), 0);

    std::cout << "Scale: " << scale << std::endl;
    glm::vec3 scaledCameraPos = cameraPosition * scale;
    camera = CPUCamera(scaledCameraPos, cameraLookAt * scale, screenWidth, screenHeight, glm::radians(30.0f), config);
    std::map<std::string, LoadedTexture> textures;

    auto cameraChunkCoords = glm::ivec2(floor(scaledCameraPos.x / maxChunkResolution),
                                        floor(scaledCameraPos.y / maxChunkResolution));

    std::filesystem::path filePath{inputFile};
    auto directory = std::format("{}_{}", (filePath.parent_path() / filePath.stem()).string(), gridSize);

    for (int y = 0; y < gridSize; y++) {
        for (int x = 0; x < gridSize; x++) {
            auto aabb = Aabb{};
            int dist = std::max(abs(y - cameraChunkCoords.y), abs(x - cameraChunkCoords.x));
            int octreeResolution = maxChunkResolution >> dist;
            std::cout << "Octree resolution: " << octreeResolution << std::endl;


            aabb.aa = glm::ivec3(x * maxChunkResolution, y * maxChunkResolution, 0);
            aabb.bb = glm::ivec3(aabb.aa.x + maxChunkResolution, aabb.aa.y + maxChunkResolution, maxChunkResolution);
            uint32_t maxDepth = std::ceil(std::log2(octreeResolution));

            uint32_t nodeAmount = 0;
            uint32_t farValuesOffset = farValues.size();
            uint32_t rootNodeIndex = octreeGPU.size();

            if (!loadChunk(directory, maxChunkResolution, octreeResolution, glm::ivec2(x, y), nodeAmount, octreeGPU,
                           farValues)) {
                std::cout << "Chunk not yet created, generating the chunk" << std::endl;
                auto node = createNode(aabb, triangles, allIndices, textures, nodeAmount, maxDepth, 0);
                // std::cout << "Finished creating nodes " << triangles.size() << std::endl;
                auto chunkFarValues = std::vector<uint32_t>();
                auto chunkOctreeGPU = std::vector<uint32_t>();
                if (node) {
                    auto shared_node = std::make_shared<OctreeNode>(*node);
                    addOctreeGPUdataBF(chunkOctreeGPU, shared_node, nodeAmount, chunkFarValues);
                    if (!saveChunk(directory, maxChunkResolution, octreeResolution, glm::ivec2(x, y), nodeAmount,
                                   chunkOctreeGPU, chunkFarValues)) {
                        std::cout << "Something went wrong storing Chunk data" << std::endl;
                    }

                    octreeGPU.insert(octreeGPU.end(), chunkOctreeGPU.begin(), chunkOctreeGPU.end());
                    farValues.insert(farValues.end(), chunkFarValues.begin(), chunkFarValues.end());
                } else {
                    rootNodeIndex = 0;
                    saveChunk(directory, maxChunkResolution, octreeResolution, glm::ivec2(x, y), nodeAmount,
                              chunkOctreeGPU, chunkFarValues);
                }
            }


            gridValues[y * gridSize + x] = Chunk{farValuesOffset, rootNodeIndex};
        }
    }
    std::cout << "Total far values: " << farValues.size() << std::endl;

    for (const auto &[key, value]: textures) {
        stbi_image_free(value.imageData);
    }
}


OctreeNode voxelizeObj(std::string inputFile, std::string path, uint32_t svo_resolution, uint32_t &nodeCount) {
    std::vector<TexturedTriangle> triangles;
    float scale;
    int result = loadObject(inputFile, path, svo_resolution, svo_resolution, triangles, scale);

    auto aabb = Aabb{};
    aabb.aa = glm::ivec3(0, 0, 0);
    aabb.bb = glm::ivec3(svo_resolution, svo_resolution, svo_resolution);
    uint32_t maxDepth = std::ceil(std::log2(svo_resolution));

    std::vector<uint32_t> allIndices(triangles.size());
    std::iota(allIndices.begin(), allIndices.end(), 0);

    std::map<std::string, LoadedTexture> textures;
    std::cout << "Creating nodes" << std::endl;
    auto node = createNode(aabb, triangles, allIndices, textures, nodeCount, maxDepth, 0);
    std::cout << "Finished creating nodes " << triangles.size() << std::endl;

    for (const auto &[key, value]: textures) {
        stbi_image_free(value.imageData);
    }

    if (node) {
        return *node;
    }

    return OctreeNode();
}
