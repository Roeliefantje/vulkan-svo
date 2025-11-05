//
// Created by roeld on 03/11/2025.
//
#include "config.h"

#include <glm/detail/func_trigonometric.inl>
#include <spdlog/spdlog.h>

#include "structures.h"

void Config::read_keyframes() {
    using json = nlohmann::json;
    namespace fs = std::filesystem;
    auto kfs = std::vector<CameraKeyFrame>();
    std::ifstream f(camera_keyframe_path);
    json data = json::parse(f);
    if (data.contains("direction")) {
        cameraDirection.x = data["direction"]["x"];
        cameraDirection.y = data["direction"]["y"];
        cameraDirection.z = data["direction"]["z"];
    }

    if (data.contains("keyframes")) {
        for (const auto &item: data["keyframes"]) {
            CameraKeyFrame kf;
            kf.time = item.at("time").get<float>();
            auto pos = item.at("position");
            auto direction = item.at("direction");
            kf.position = glm::vec3(pos[0], pos[1], pos[2]);
            kf.direction = glm::vec3(direction[0], direction[1], direction[2]);
            kfs.push_back(kf);
        }
    }

    cameraKeyFrames = kfs;
}

Config::Config(int argc, char *argv[]) {
    using json = nlohmann::json;
    namespace fs = std::filesystem;
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
    chunkgen = result["chunkgen"].as<bool>();
    if (result.count("test")) {
        printChunkDebug = false;
        allowUserInput = false;
        loglevel = spdlog::level::info;
        //Setup camera stuff
        switch (auto test_scene = result["test"].as<uint32_t>()) {
            case 0:
                camera_path = "./camera_positions/camera_location_test0.json";
                spdlog::info("Test Scene 0 Benchmark!");
                break;
            case 1:
                cameraKeyFrames = std::vector<CameraKeyFrame>();
                camera_keyframe_path = "./camera_positions/camera_path_test2.json";
                read_keyframes();
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
}
