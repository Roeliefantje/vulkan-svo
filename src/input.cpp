//
// Created by roeld on 19/10/2025.
//

#define GLM_ENABLE_EXPERIMENTAL
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include "compute_shader_application.h"

void mouseCallback(GLFWwindow *window, double xpos, double ypos) {
    auto *app = static_cast<ComputeShaderApplication *>(glfwGetWindowUserPointer(window));
    if (app) {
        if (app->lastFrameTime == 0.0f) {
            app->mouseX = xpos;
            app->mouseY = ypos;
            return;
        }

        float xoffset = xpos - app->mouseX;
        float yoffset = app->mouseY - ypos;
        app->mouseX = xpos;
        app->mouseY = ypos;

        //     std::cout << "X offset:" << xoffset << std::endl;
        //     std::cout << "Y offset:" << yoffset << std::endl;

        if (!app->mouseFree && (xoffset != 0.0f || yoffset != 0.0f)) {
            glm::vec3 right = glm::cross(app->camera.direction, app->camera.up);
            float angleR = glm::radians(yoffset * app->lastFrameTime * app->mouseSensitivity);
            glm::vec3 rotated = glm::rotate(app->camera.direction, angleR, right);

            // std::cout << "Original direction: X: " << app->camera.direction.x << " Y:" << app->camera.direction.y <<
            //         " Z:" << app->camera.direction.z << std::endl;
            // std::cout << "Rotated: X: " << rotated.x << " Y:" << rotated.y << " Z:" << rotated.z << std::endl;

            if (rotated.z <= 0.9 && rotated.z >= -0.9) {
                app->camera.direction = rotated;
            }

            angleR = glm::radians(xoffset * app->lastFrameTime * app->mouseSensitivity);
            app->camera.direction = glm::rotate(app->camera.direction, angleR, app->camera.up);
        }
    }
}
