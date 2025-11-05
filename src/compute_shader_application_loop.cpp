//
// Created by roeld on 03/11/2025.
//
#include "compute_shader_application.h"
#include "spdlog/spdlog.h"

CameraKeyFrame interpolateCamera(const std::vector<CameraKeyFrame> &keyframes, const float currentTime) {
    if (keyframes.empty()) return {};
    if (currentTime <= keyframes.front().time) return keyframes.front();
    if (currentTime >= keyframes.back().time) return keyframes.back();

    // Find interval [i, i+1]
    for (size_t i = 0; i < keyframes.size() - 1; ++i) {
        const auto &kf1 = keyframes[i];
        const auto &kf2 = keyframes[i + 1];

        if (currentTime >= kf1.time && currentTime <= kf2.time) {
            float t = (currentTime - kf1.time) / (kf2.time - kf1.time);
            glm::vec3 position = glm::mix(kf1.position, kf2.position, t);
            glm::vec3 direction = glm::slerp(kf1.direction, kf2.direction, t);
            return {currentTime, position, direction};
        }
    }

    return keyframes.back();
}

void ComputeShaderApplication::mainLoop() {
    double lastPrint = glfwGetTime();
    while (!glfwWindowShouldClose(window)) {
        checkChunks(cpuGridValues, camera, config.chunk_resolution, *dmThreat);
        if (config.allowUserInput) {
            processInput();
        }
        glfwPollEvents();
        drawFrame();
        static const auto startTime = glfwGetTime();
        double currentTime = glfwGetTime();
        double elapsed = currentTime - lastPrint;

        if (config.cameraKeyFrames) {
            double totalElapsed = glfwGetTime() - startTime;
            auto kf = interpolateCamera(config.cameraKeyFrames.value(), totalElapsed);
            camera.updatePosition(kf.position);
            camera.gpu_camera.direction = glm::normalize(kf.direction);
        }

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
