#include "src/compute_shader_application.h"

int main(int argc, char *argv[]) {
    ComputeShaderApplication app;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
