#include "src/compute_shader_application.h"
#include "src/config.h"

int main(int argc, char *argv[]) {
    Config config{argc, argv};
    ComputeShaderApplication app{config};

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
