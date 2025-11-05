#include "src/chunk_generation_application.h"
#include "src/compute_shader_application.h"
#include "src/config.h"
#include <spdlog/spdlog.h>

int main(int argc, char *argv[]) {
    spdlog::set_level(spdlog::level::debug);
    spdlog::set_pattern("[%H:%M:%S.%e] [%l] [thread %t] %v");
    Config config{argc, argv};
    spdlog::set_level(config.loglevel);
    if (config.chunkgen) {
        ChunkGenerationApplication app{config};
        app.genererateChunks();
        return EXIT_SUCCESS;
    }


    try {
        if (config.chunkgen) {
            ChunkGenerationApplication app{config};
            app.genererateChunks();
        } else {
            ComputeShaderApplication app{config};
            app.run();
        }
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
