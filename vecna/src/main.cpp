// Vecna - 3D Model Viewer
// Entry point

#include "Vecna/Core.hpp"

#include <cstdlib>
#include <exception>
#include <iostream>

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) {
    try {
        Vecna::Core::Application app;
        app.run();
        return EXIT_SUCCESS;
    } catch (const std::exception& e) {
        std::cerr << "[Core] Fatal error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
}
