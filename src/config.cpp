//
// Created by roeld on 03/11/2025.
//
#include "config.h"

#include <glm/detail/func_trigonometric.inl>


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
}
