//
// Created by roeld on 03/11/2025.
//
#include "compute_shader_application.h"
#include "spdlog/spdlog.h"

void ComputeShaderApplication::mainLoop() {
    double lastPrint = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        static const auto startTime = glfwGetTime();
        if (config.cameraKeyFrames) {
            double totalElapsed = glfwGetTime() - startTime;
            auto kf = interpolateCamera(config.cameraKeyFrames.value(), static_cast<float>(totalElapsed));
            if (objSceneMetaData) {
                camera.setPosition(kf.position * objSceneMetaData->scale);
            } else {
                spdlog::debug("Position:, {}, {}, {}", kf.position.x, kf.position.y, kf.position.z);
                camera.setPosition(kf.position);
            }
            camera.gpu_camera.direction = glm::normalize(kf.direction);
        }
        if (config.allowUserInput) {
            processInput();
        }
        checkChunks(cpuGridValues, camera, config.chunk_resolution, *dmThreat);
        glfwPollEvents();
        drawFrame();
        double currentTime = glfwGetTime();
        double elapsed = currentTime - lastPrint;


        if (elapsed >= 1.0) {
            // printCameraPosition();
            spdlog::info("FPS: {}, msPf: {}", (frameCounter / elapsed), 1000 / (frameCounter / elapsed));
            farValuesGPUManager->printBufferInfo();
            octreeGPUManager->printBufferInfo();
            stagingBufferManager->printBufferInfo();
#if SHADERDEBUG
            spdlog::info("Average Steps per ray: {}", (totalSteps / (float) (config.width * config.height)));
            spdlog::info("Max Steps per ray: {}", maxSteps);
#endif

            frameCounter = 0;
            lastPrint = currentTime;
        }
        lastFrameTime = (currentTime - lastTime) * 1000.0;
        lastTime = currentTime;
        frameCounter++;
    }

    vkDeviceWaitIdle(device);
}
