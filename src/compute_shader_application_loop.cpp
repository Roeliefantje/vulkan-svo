//
// Created by roeld on 03/11/2025.
//
#include "compute_shader_application.h"

void ComputeShaderApplication::mainLoop() {
    double lastPrint = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        checkChunks(cpuGridValues, camera, config.chunk_resolution, *dmThreat);
        processInput();
        glfwPollEvents();
        drawFrame();
        double currentTime = glfwGetTime();
        double elapsed = currentTime - lastPrint;
#ifdef DEBUG
        if (elapsed >= 1.0 && false) {
            std::cout << "FPS: " << (frameCounter / elapsed) << "\n";
#if SHADERDEBUG
            std::cout << "Average Steps per ray: " << (totalSteps / (float) (config.width * config.height)) << "\n";
            std::cout << "Max Steps per ray: " << maxSteps << "\n";
#endif

            frameCounter = 0;
            lastPrint = currentTime;
        }
#endif
        lastFrameTime = (currentTime - lastTime) * 1000.0;
        lastTime = currentTime;
        frameCounter++;
    }

    vkDeviceWaitIdle(device);
}
