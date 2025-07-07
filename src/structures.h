#pragma once

#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <vector>
#include <random>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct UniformBufferObject {
    float deltaTime = 1.0f;
};

struct Vertex {
    glm::vec2 position;
    glm::vec2 texCoord;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, position);

        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }
};

const uint32_t VOXEL_COUNT = 500000;

struct GridInfo {
    alignas(4) uint32_t width;
    alignas(4) uint32_t height;
    alignas(4) uint32_t depth;

    GridInfo(uint32_t w, uint32_t h, uint32_t d) : width(w), height(h), depth(d) {
    }
};

struct Grid {
    GridInfo gridInfo;
    std::vector<int32_t> data;

    Grid(uint32_t w, uint32_t h, uint32_t d) : gridInfo(w, h, d), data(w * d * h) {
        //Do stuff with data
        std::default_random_engine rndEngine((unsigned) time(nullptr));
        // std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);
        std::uniform_int_distribution<int> rndWidth(0, w - 1);
        std::uniform_int_distribution<int> rndHeight(0, h - 1);
        std::uniform_int_distribution<int> rndDepth(0, d - 1);

        for (size_t i = 0; i < VOXEL_COUNT; i++) {
            auto x = rndDepth(rndEngine);
            auto y = rndWidth(rndEngine);
            auto z = rndHeight(rndEngine);
            // auto z = 0;
            auto index = x + y * gridInfo.width + z * gridInfo.width * gridInfo.height;
            data[index] = 1;
        }

        std::cout << "Finished making grid" << std::endl;
    }
};

struct Camera {
    alignas(16) glm::vec3 position;
    alignas(16) glm::vec3 direction;
    alignas(16) glm::vec3 up;
    alignas(8) glm::vec2 resolution;
    alignas(4) float fov;

    //Technically, adding fov to a vec3 would save some memory. (it could all fit in 3 vec4s)
    Camera(glm::vec3 pos, glm::vec3 lookAt, int screenWidth, int screenHeight, float fovRadian) : position(pos),
        fov(fovRadian) {
        direction = glm::normalize(lookAt - pos);
        up = glm::vec3(0, 0, 1);
        resolution = glm::vec2(screenWidth, screenHeight);

        if (direction == up) {
            throw std::runtime_error("Camera up vector is the same as Camera direction!");
        }
    }
};


#endif //STRUCTURES_H
