//
// Created by roeld on 01/10/2025.
//
#pragma once

#ifndef CONFIG_H
#define CONFIG_H
#include <cxxopts.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
namespace fs = std::filesystem;

struct Config {
    //Whether we use the voxelizer or the heightmap data.
    bool useHeightmapData = true;

    //Output screen size
    uint32_t width = 1920;
    uint32_t height = 1080;

    //What value are we using for the gpu compute groups
    float x_groupsize = 16;
    float y_groupsize = 16;
    size_t GIGABYTE = (1 << 30);

    VkDeviceSize staging_size = GIGABYTE << 1;
    uint32_t chunk_resolution = 1024;
    uint32_t grid_size = 21;
    uint32_t seed = 12345 * 5;

    //MouseSensitivity
    float mouseSensitivity = 0.01f;
    //The default camera positions dont really get used, its rather just a fallback.
    glm::vec3 cameraPosition = glm::vec3(100 + chunk_resolution, 100 + chunk_resolution, 712.5);
    glm::vec3 cameraDirection = glm::vec3(0.5, 0.5, 0);
    float fov = glm::radians(30.0f);
    std::string scene_path = "./assets/san-miguel-low-poly.obj";
    std::string camera_path = "./camera-hm.json";


    Config(int argc, char *argv[]) {
        cxxopts::Options options("./program", "Sparse Voxel Octree Terrain Renderer");
        options.add_options()
                ("d,debug", "Enable debugging") // a bool parameter
                ("voxelize", "path to scene to voxelize", cxxopts::value<std::string>())
                ("g, grid", "The grid size for the scene", cxxopts::value<uint32_t>())
                ("s, seed", "Seed to use for chunk generation", cxxopts::value<uint32_t>())
                ("t, test", "Which test scenario to run", cxxopts::value<uint32_t>())
                ("chunkgen", "Generate chunks needed for a certain camera position",
                 cxxopts::value<bool>()->default_value("false"))
                ("c, camera", "Camera position for the float location", cxxopts::value<std::string>())
                ("campath", "Make the camera follow a set path")
                ("h, help", "Print how to use the program");
        // ();

        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            exit(0);
        }
        bool chunkgen = result["chunkgen"].as<bool>();
        if (result.count("test")) {
            //Setup camera stuff
            auto test_scene = result["test"].as<uint32_t>();
            switch (test_scene) {
                case 0:
                    std::cout << "Test Scene 0 Benchmark!" << std::endl;
                    break;
                default:
                    std::cout << "Unknown test scene Benchmark!" << std::endl;
                    exit(0);
            }
        }

        if (result.count("camera")) {
            camera_path = result["voxelize"].as<std::string>();
        }

        if (result.count("voxelize")) {
            scene_path = result["voxelize"].as<std::string>();
        }

        //Parse the camera json
        std::ifstream f(camera_path);
        json data = json::parse(f);
        if (data.contains("position")) {
            cameraPosition.x = data["position"]["x"];
            cameraPosition.y = data["position"]["y"];
            cameraPosition.z = data["position"]["z"];
        }
        if (data.contains("direction")) {
            cameraDirection.x = data["direction"]["x"];
            cameraDirection.y = data["direction"]["y"];
            cameraDirection.z = data["direction"]["z"];
        }
        if (data.contains("fov")) {
            fov = glm::radians(data["fov"].get<float>());
        }

        if (chunkgen) {
            //Do certain stuff ig
        }
    }
};

#endif //CONFIG_H
