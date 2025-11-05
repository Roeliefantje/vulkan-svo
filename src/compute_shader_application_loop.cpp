//
// Created by roeld on 03/11/2025.
//
#include "compute_shader_application.h"
#include "spdlog/spdlog.h"

void ComputeShaderApplication::mainLoop() {
    double lastPrint = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        checkChunks(cpuGridValues, camera, config.chunk_resolution, *dmThreat);
        if (config.allowUserInput) {
            processInput();
        }
        glfwPollEvents();
        drawFrame();
        double currentTime = glfwGetTime();
        double elapsed = currentTime - lastPrint;
        if (elapsed >= 1.0) {
            spdlog::info("FPS: {}, msPf: {}", (frameCounter / elapsed), 1000 / (frameCounter / elapsed));
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
