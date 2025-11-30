# Setting up and Installing the software
To setup the software Cmake is used, required system libraries are Vulkan, Glfw and glm.

# Running the experiments
To run the experiments, first ensure the program is build and executes properly, once this is the case setup the python environment using uv.
Be sure to download the assets used for the experiments by running the download_assets.py file.
Run run_experiments.py
The figures should be generated

# Running the LOD and Rays per pixel shader
To run the different shaders, the defines in shader.comp need to be uncommented and the  shader needs to be compiled again.
