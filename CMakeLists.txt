cmake_minimum_required(VERSION 3.31)
project(clion_vulkan)

set(CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)


add_executable(clion_vulkan main.cpp)

target_include_directories(clion_vulkan PRIVATE ${Vulkan_INCLUDE_DIRS})
target_include_directories(clion_vulkan PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(clion_vulkan PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(clion_vulkan PRIVATE glfw)
target_link_libraries(clion_vulkan PRIVATE glm::glm-header-only)