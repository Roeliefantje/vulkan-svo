//
// Created by roeld on 01/10/2025.
//
#pragma once

#ifndef CONFIG_H
#define CONFIG_H
#include <cxxopts.hpp>


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
    uint32_t chunk_resolution = 1024 * 4;
    uint32_t grid_size = 6;
    uint32_t seed = 12345 * 5;

    //MouseSensitivity
    float mouseSensitivity = 0.01f;


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
