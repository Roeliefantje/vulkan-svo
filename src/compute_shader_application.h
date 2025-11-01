//
// Created by roeld on 19/10/2025.
//
#pragma once

#ifndef COMPUTE_SHADER_APPLICATION_H
#define COMPUTE_SHADER_APPLICATION_H

//TODO!: Change LookAt camera uniform to look direction instead of lookAt, but creation should still be with lookAt.
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/rotate_vector.hpp>


#include <iostream>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <chrono>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <array>
#include <atomic>
#include <optional>
#include <set>

#include "config.h"
#include "data_manage_threat.h"
#include "structures.h"
#include "scene_metadata.h"

#define SHADERDEBUG 1
#define DEBUG 1

const int MAX_FRAMES_IN_FLIGHT = 1;
const int MAX_VOXEL_BUFFERS = 32;

const std::vector<const char *> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char *> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
};

#define NDEBUG
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator,
                                      VkDebugUtilsMessengerEXT *pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator);

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsAndComputeFamily;
    std::optional<uint32_t> presentFamily;
    std::optional<uint32_t> transferFamily;

    bool isComplete();
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};


const std::vector<Vertex> vertices = {
    {{-1.0f, -1.0f}, {1.0f, 0.0f}},
    {{1.0f, -1.0f}, {0.0f, 0.0f}},
    {{1.0f, 1.0f}, {0.0f, 1.0f}},
    {{-1.0f, 1.0f}, {1.0f, 1.0f}},
};

const std::vector<uint16_t> indices = {
    0, 2, 1, 2, 0, 3 // counter-clockwise
    // 0, 1, 2, 2, 3, 0
};


class ComputeShaderApplication {
public:
    ComputeShaderApplication(Config config);

    void run();

    CPUCamera camera;
    Config config;

    float mouseX = 0;
    float mouseY = 0;
    bool escPressed = false;
    bool mouseFree = false;

    float lastFrameTime = 0.0f;

private:
    GLFWwindow *window;

    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
    VkSurfaceKHR surface;

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device;


    VkQueue graphicsQueue;
    VkQueue computeQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapChain;
    std::vector<VkImage> swapChainImages;
    VkFormat swapChainImageFormat;
    VkExtent2D swapChainExtent;
    std::vector<VkImageView> swapChainImageViews;
    std::vector<VkFramebuffer> swapChainFramebuffers;

    VkRenderPass renderPass;
    VkDescriptorSetLayout graphicsDescriptorSetLayout;
    VkPipelineLayout pipelineLayout;
    VkPipeline graphicsPipeline;

    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline;

    VkCommandPool commandPool;


    std::vector<std::vector<VkBuffer> > shaderStorageBuffers;
    std::vector<VkBuffer> farValuesSBuffers;
    std::vector<VkBuffer> gridBuffers;
    std::vector<VkBuffer> debugBuffers;
    std::vector<std::vector<VkDeviceMemory> > shaderStorageBuffersMemory;
    std::vector<VkDeviceMemory> farValuesSBuffersMemory;
    std::vector<VkDeviceMemory> gridBuffersMemory;
    std::vector<VkDeviceMemory> debugBuffersMemory;

    StagingBufferProperties stagingBufferProperties;

    //Add the other stuff
    std::vector<VkBuffer> uniformBuffers;
    std::vector<VkDeviceMemory> uniformBuffersMemory;
    std::vector<void *> uniformBuffersMapped;

    std::vector<VkBuffer> uniformCameraBuffers;
    std::vector<VkDeviceMemory> uniformCameraBuffersMemory;
    std::vector<void *> uniformCameraBuffersMapped;

    VkBuffer vertexBuffer;
    VkDeviceMemory vertexBufferMemory;

    VkBuffer indexBuffer;
    VkDeviceMemory indexBufferMemory;

    VkImage computeImage;
    VkDeviceMemory computeImageMemory;
    VkImageView computeImageView;
    VkSampler computeImageSampler;

    VkDescriptorPool graphicsDescriptorPool;
    VkDescriptorPool computeDescriptorPool;
    std::vector<VkDescriptorSet> computeDescriptorSets;
    std::vector<VkDescriptorSet> graphicsDescriptorSets;

    std::vector<VkCommandBuffer> commandBuffers;
    std::vector<VkCommandBuffer> computeCommandBuffers;

    std::vector<VkSemaphore> imageAvailableSemaphores;
    std::vector<VkSemaphore> renderFinishedSemaphores;
    std::vector<VkSemaphore> computeFinishedSemaphores;
    std::vector<VkFence> inFlightFences;
    std::vector<VkFence> computeInFlightFences;
    uint32_t currentFrame = 0;
    uint32_t maxBufferSize = 0;

    //TODO: Rename gridinfo to proper name...
    GridInfo gridInfo;
    uint32_t amountOfNodes = 0;
    std::vector<uint32_t> farValues;
    std::vector<uint32_t> octreeGPU;
    std::vector<Chunk> gridValues;
    std::vector<CpuChunk> cpuGridValues;
    VkFence renderingFence;


    bool framebufferResized = false;

    double lastTime = 0.0f;
    int frameCounter = 0;

    //Debug stuff
    uint32_t totalSteps;
    uint32_t maxSteps;

    BufferManager *octreeGPUManager;
    BufferManager *farValuesGPUManager;
    DataManageThreat *dmThreat;
    std::optional<SceneMetadata> objSceneMetaData = std::nullopt;


    void initData();

    void initWindow();

    static void framebufferResizeCallback(GLFWwindow *window, int width, int height);

    void initVulkan();

    //std::vector<VkBuffer> farValuesSBuffers;
    //std::vector<VkBuffer> gridBuffers;
    void initThreads();

    void initCamera();

    void mainLoop();

    void cleanupSwapChain();

    void cleanup();

    //Has to be static for Glfw
    static void mouseCallback(GLFWwindow *window, double xpos, double ypos);

    void processInput();

    void updateUniformCameraBuffer();

    void recreateSwapChain();

    void createInstance();

    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);

    void setupDebugMessenger();

    void createSurface();

    void pickPhysicalDevice();

    void createLogicalDevice();

    void getMaxBufferSize();

    void createSwapChain();

    void createImageViews();

    void createRenderPass();

    void createGraphicsDescriptorSetLayout();

    void createComputeDescriptorSetLayout();

    void createGraphicsPipeline();

    void createComputePipeline();

    void createFramebuffers();

    void createCommandPool();

    void createTransferCommandPool();

    void createStagingBuffer(VkDeviceSize size);

    void createShaderStorageBuffer(std::vector<uint32_t> &dataVec, std::vector<std::vector<VkBuffer> > &buffers,
                                   std::vector<std::vector<VkDeviceMemory> > &buffersMemory);

    void createDebugShaderStorageBuffer();

    template<typename T>
    void createSingleShaderStorageBuffer(std::vector<T> &dataVec, std::vector<VkBuffer> &buffers,
                                         std::vector<VkDeviceMemory> &buffersMemory);

    void createShaderStorageBuffers();

    void createUniformBuffers();

    void createGraphicsDescriptorPool();

    void createComputeDescriptorPool();

    void createComputeImage();

    void createGraphicsDescriptorSets();

    void createComputeDescriptorSets();


    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer,
                      VkDeviceMemory &bufferMemory);

    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

    void createCommandBuffers();

    void createComputeCommandBuffers();

    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    void recordComputeCommandBuffer(VkCommandBuffer commandBuffer, uint32_t current_frame);

    void createVertexBuffer();

    void drawFrame();

    void createIndexBuffer();

    void createSyncObjects();

    VkShaderModule createShaderModule(const std::vector<char> &code);

    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);

    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);

    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

    bool isDeviceSuitable(VkPhysicalDevice device);

    bool checkDeviceExtensionSupport(VkPhysicalDevice device);

    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

    std::vector<const char *> getRequiredExtensions();

    bool checkValidationLayerSupport();

    VkCommandBuffer beginSingleTimeCommands();

    void endSingleTimeCommands(VkCommandBuffer commandBuffer);

    void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

    void printCameraPosition();

    static std::vector<char> readFile(const std::string &filename);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                        VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
                                                        void *pUserData);
};

#endif //COMPUTE_SHADER_APPLICATION_H
