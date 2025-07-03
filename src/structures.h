#pragma once

#ifndef STRUCTURES_H
#define STRUCTURES_H

#include <vector>
#include <random>

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

const uint32_t VOXEL_COUNT = 5000;

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
        std::uniform_int_distribution<int> rndWidth(0, w);
        std::uniform_int_distribution<int> rndHeight(0, h);
        std::uniform_int_distribution<int> rndDepth(0, d);

        for (size_t i = 0; i < VOXEL_COUNT; i++) {
            auto x = rndWidth(rndEngine);
            auto y = rndDepth(rndEngine);
            // auto z = rndHeight(rndEngine);
            auto z = 0;
            auto index = x + gridInfo.depth * y + gridInfo.height * gridInfo.depth * z;
            data[index] = 1;
        }
        // // Initial particle positions on a circle
        // std::vector<Particle> particles(PARTICLE_COUNT);
        // for (auto &particle: particles) {
        //     float r = 0.25f * sqrt(rndDist(rndEngine));
        //     float theta = rndDist(rndEngine) * 2.0f * 3.14159265358979323846f;
        //     float x = r * cos(theta) * HEIGHT / WIDTH;
        //     float y = r * sin(theta);
        //     particle.position = glm::vec2(x, y);
        //     particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.00025f;
        //     particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
        // }
    }
};

// struct Particle {
//     glm::vec2 position;
//     glm::vec2 velocity;
//     glm::vec4 color;
//
//     static VkVertexInputBindingDescription getBindingDescription() {
//         VkVertexInputBindingDescription bindingDescription{};
//         bindingDescription.binding = 0;
//         bindingDescription.stride = sizeof(Particle);
//         bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
//
//         return bindingDescription;
//     }
//
//     static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
//         std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
//
//         attributeDescriptions[0].binding = 0;
//         attributeDescriptions[0].location = 0;
//         attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
//         attributeDescriptions[0].offset = offsetof(Particle, position);
//
//         attributeDescriptions[1].binding = 0;
//         attributeDescriptions[1].location = 1;
//         attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
//         attributeDescriptions[1].offset = offsetof(Particle, color);
//
//         return attributeDescriptions;
//     }
// };


#endif //STRUCTURES_H
