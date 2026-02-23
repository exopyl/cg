# Vecna

A cross-platform 3D model viewer built with Vulkan.

## Features

- Load and visualize OBJ and STL 3D models
- Trackball camera navigation (rotate, zoom, pan)
- Display model information (bounding box, vertex/face count)
- Cross-platform: Windows, Linux, macOS

## Requirements

- CMake 3.20+
- C++20 compiler:
  - GCC 10+ (Linux)
  - Clang 10+ (Linux/macOS)
  - MSVC 19.29+ (Windows)
- Vulkan SDK (includes glslc for shader compilation)
  - On macOS, MoltenVK is included in the SDK

## Building

```bash
# Configure
cmake -B build

# Build
cmake --build build

# Run
./build/vecna        # Linux/macOS
.\build\vecna.exe    # Windows
```

## Project Structure

```
vecna/
├── CMakeLists.txt          # Root CMake configuration
├── cmake/                  # CMake modules
│   ├── CompilerWarnings.cmake
│   ├── FetchDependencies.cmake
│   └── ShaderCompilation.cmake
├── include/Vecna/          # Public headers
├── src/                    # Source files
├── shaders/                # GLSL shaders
├── tests/                  # Unit tests
└── assets/                 # Test models
```

## Dependencies

Managed via CMake FetchContent:
- [GLFW](https://github.com/glfw/glfw) - Windowing
- [VMA](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator) - Vulkan memory management
- [Dear ImGui](https://github.com/ocornut/imgui) - UI framework

## License

Personal learning project.
