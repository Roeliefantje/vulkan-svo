//
// Created by roeld on 23/10/2025.
//
#include "compute_shader_application.h"

void ComputeShaderApplication::mouseCallback(GLFWwindow *window, double xpos, double ypos) {
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
            glm::vec3 right = glm::cross(app->camera.gpu_camera.direction, app->camera.gpu_camera.up);
            float angleR = glm::radians(yoffset * app->lastFrameTime * app->config.mouseSensitivity);
            glm::vec3 rotated = glm::rotate(app->camera.gpu_camera.direction, angleR, right);

            // std::cout << "Original direction: X: " << app->camera.direction.x << " Y:" << app->camera.direction.y <<
            //         " Z:" << app->camera.direction.z << std::endl;
            // std::cout << "Rotated: X: " << rotated.x << " Y:" << rotated.y << " Z:" << rotated.z << std::endl;

            if (rotated.z <= 0.9 && rotated.z >= -0.9) {
                app->camera.gpu_camera.direction = rotated;
            }

            angleR = glm::radians(xoffset * app->lastFrameTime * app->config.mouseSensitivity);
            app->camera.gpu_camera.direction = glm::rotate(app->camera.gpu_camera.direction, angleR,
                                                           app->camera.gpu_camera.up);
        }
    }
}

void ComputeShaderApplication::processInput() {
    glm::vec3 forward = glm::normalize(
        camera.gpu_camera.direction - glm::dot(camera.gpu_camera.direction, camera.gpu_camera.up) * camera.gpu_camera.
        up);
    glm::vec3 left = glm::cross(forward, camera.gpu_camera.up);
    if (!mouseFree) {
        auto multiplier = 0.2f;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) {
            multiplier *= 10.0f;
        }
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
            glm::vec3 change = forward * multiplier * lastFrameTime;
            camera.updatePosition(change);
            // camera.lookAt += change;
        }
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
            glm::vec3 change = forward * -1.0f * multiplier * lastFrameTime;
            camera.updatePosition(change);
            // camera.lookAt += change;
        }
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
            glm::vec3 change = left * multiplier * lastFrameTime;
            camera.updatePosition(change);
            // camera.lookAt += change;
        }
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
            glm::vec3 change = left * -1.0f * multiplier * lastFrameTime;
            camera.updatePosition(change);
            // camera.lookAt += change;
        }
        if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
            glm::vec3 change = camera.gpu_camera.up * multiplier * lastFrameTime;
            camera.updatePosition(change);
            // camera.lookAt += change;
        }
        if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) {
            glm::vec3 change = camera.gpu_camera.up * -1.0f * multiplier * lastFrameTime;
            camera.updatePosition(change);
            // camera.lookAt += change;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        if (!escPressed) {
            std::cout << "Escape pressed" << std::endl;
            if (!mouseFree) {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            } else {
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            mouseFree = !mouseFree;
        }
        escPressed = true;
    } else if (escPressed) {
        escPressed = false;
    }
}
