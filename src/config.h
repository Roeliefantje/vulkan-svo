//
// Created by roeld on 01/10/2025.
//

#ifndef CONFIG_H
#define CONFIG_H
#include <cxxopts.hpp>

struct Config {
    bool useHeightmapData = true;
    uint32_t WIDTH = 1920 * 2;
    uint32_t HEIGHT = 1080;
    float X_GROUPSIZE = 16;
    float Y_GROUPSIZE = 16;
    size_t GIGABYTE = (1 << 30);
    VkDeviceSize STAGING_SIZE = GIGABYTE << 1;
    uint32_t CHUNK_RESOLUTION = 1024 * 4;
    uint32_t GRID_SIZE = 6;
    uint32_t SEED = 12345 * 5;


    Config(int argc, char *argv[]) {
        cxxopts::Options options("Sparse Voxel Octree Renderer", "One line description of MyProgram");
        options.add_options()
                ("d,debug", "Enable debugging") // a bool parameter
                ("voxelize", "Voxelize a scene", cxxopts::value<std::string>())
                ("g, grid", "The grid size for the scene", cxxopts::value<uint32_t>())
                ("s, seed", "Seed to use for chunk generation", cxxopts::value<uint32_t>())
                // ("c, camera", "Camera position for the float location", cxxopts::value<float>())
                ("campath", "Make the camera follow a set path");
        // ();

        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help() << std::endl;
            exit(0);
        }
    }
};

#endif //CONFIG_H
