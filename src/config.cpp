//
// Created by roeld on 03/11/2025.
//
#include "config.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <spdlog/spdlog.h>

#include "svo_generation.h"
#include "structures.h"

void Config::read_keyframes() {
    using json = nlohmann::json;
    namespace fs = std::filesystem;
    auto kfs = std::vector<CameraKeyFrame>();
    std::ifstream f(camera_keyframe_path);
    json data = json::parse(f);
    if (data.contains("fov")) {
        fov = glm::radians(data["fov"].get<float>());
    }

    if (data.contains("keyframes")) {
        for (const auto &item: data["keyframes"]) {
            CameraKeyFrame kf{};
            kf.time = item.at("time").get<float>();
            auto pos = item.at("position");
            auto direction = item.at("direction");
            kf.position = glm::vec3(pos[0], pos[1], pos[2]);
            kf.direction = glm::vec3(direction[0], direction[1], direction[2]);
            kfs.push_back(kf);
        }

        cameraPosition = kfs[0].position;
        cameraDirection = kfs[0].direction;
    }

    cameraKeyFrames = kfs;
}

void Config::generate_keyframes() {
    int totalHeight = grid_height * chunk_resolution;
    const int pathLength = 100;
    float duration = 60.0;
    auto cameraPosI = glm::ivec2(cameraPosition.x, cameraPosition.y);
    const glm::vec2 direction = glm::normalize(glm::vec2(cameraDirection.x, cameraDirection.y));
    std::vector<float> noise = createPathNoise(1000, seed, cameraPosI, chunk_resolution, direction);

    auto kfs = std::vector<CameraKeyFrame>();
    float currentTime = 0.0f;
    auto currentCameraPos = glm::vec2(cameraPosition);
    const float timeIncrement = duration / pathLength;
    for (const auto &height: noise) {
        CameraKeyFrame kf{};
        kf.time = currentTime;
        kf.position = glm::vec3(currentCameraPos.x, currentCameraPos.y, totalHeight * height + 100);
        kf.direction = cameraDirection;
        kfs.push_back(kf);

        currentTime += timeIncrement;
        currentCameraPos += direction;
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
            ("gridheight", "The grid height for the scene", cxxopts::value<uint32_t>())
            ("s, seed", "Seed to use for chunk generation", cxxopts::value<uint32_t>())
            ("r, res", "Max chunk resolution to use (must be a power of 2)", cxxopts::value<uint32_t>())
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
                scene_path = "./assets/san-miguel-low-poly.obj";
                useHeightmapData = false;
                spdlog::info("Test Scene 0 Benchmark!");
                break;
            case 1:
                camera_path = "./camera_positions/camera_location_test1.json";
                scene_path = "./assets/san-miguel-low-poly.obj";
                useHeightmapData = false;
                spdlog::info("Test Scene 1 Benchmark!");
                break;
            case 2:
                useHeightmapData = false;
                cameraKeyFrames = std::vector<CameraKeyFrame>();
                camera_keyframe_path = "./camera_positions/camera_path_test2.json";
                read_keyframes();
                break;
            case 3:
                useHeightmapData = true;
                cameraKeyFrames = std::vector<CameraKeyFrame>();
                generate_keyframes();
                break;
            default:
                std::cout << "Unknown test scene Benchmark!" << std::endl;
                exit(0);
        }
    }

    if (result.count("camera")) {
        camera_path = result["camera"].as<std::string>();
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
        cameraDirection = glm::normalize(cameraDirection);
    }

    if (data.contains("fov")) {
        fov = glm::radians(data["fov"].get<float>());
    }


    if (result.count("voxelize")) {
        scene_path = result["voxelize"].as<std::string>();
    }

    if (result.count("grid")) {
        grid_size = result["grid"].as<uint32_t>();
    }

    if (result.count("gridheight")) {
        grid_height = result["gridheight"].as<uint32_t>();
    }

    if (result.count("res")) {
        chunk_resolution = result["res"].as<uint32_t>();
    }


    if (grid_height > grid_size / 2) {
        //Todo!: Fix chunkload logic to properly account for any gridheight :)
        spdlog::error("Grid Height is too high compared to grid Size, might not load every chunk properly!");
    }

    if (useHeightmapData) {
        cameraPosition /= voxelscale;
    }
}

//TODO: Move this to a more proper place.
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
