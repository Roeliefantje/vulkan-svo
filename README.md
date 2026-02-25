# Vulkan Voxel ray marcher
This repository features a voxel ray marcher that renders a seemingly infinite voxel grid to the screen. This is done by utilizing a vulkan compute shader.
The rendered scene is represented with chunks, that can be loaded in at different levels of detail based on the distance to the camera.

## Level of Detail
Chunks are represented using an sparse voxel octree, when a lower level of detail is requested, we do not load the lower levels of the tree and instead take the subdivded nodes as leaf nodes.

## Chunk generation program
The chunk generation is currently not fast enough to support the high amount of voxels needed for sub-pixel voxel resolutions, to ensure we can still test the graphics engine in these setups,
a chunk generation program is written. This generates the chunks prior to running the actual program, storing them on disk for easy lookup.

## Voxelizer
The program also features voxelizer functionality in order to render any arbitrary .obj file, allowing us to render any environment with the graphics engine.

# Preview

[![Walking setup](/preview/walking-config.png)](https://www.youtube.com/watch?v=cJkdcKC7RkE)

[![Flying setup](/preview/flying-config.png)](https://www.youtube.com/watch?v=XxxMiX9JfBA)

[![Walking setup](/preview/first-location-sm.png)](https://www.youtube.com/watch?v=XxxMiX9JfBA)


# Setting up and Installing the software
To setup the software Cmake is used, required system libraries are Vulkan, Glfw and glm.

# Running the experiments
To run the experiments, first ensure the program is build and executes properly, once this is the case setup the python environment using uv.
Be sure to download the assets used for the experiments by running the download_assets.py file.
Run run_experiments.py
The figures should be generated

# Running the LOD and Rays per pixel shader
To run the different shaders, the defines in shader.comp need to be uncommented and the  shader needs to be compiled again.
