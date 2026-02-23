cmake_minimum_required(VERSION 3.20)

project(vecna
    VERSION 0.1.0
    DESCRIPTION "Cross-platform 3D model viewer with Vulkan"
    LANGUAGES CXX
)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile_commands.json for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type defaults to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Compiler warnings
include(CompilerWarnings)

# Fetch dependencies (GLFW, VMA, ImGui)
include(FetchDependencies)

# Find Vulkan
find_package(Vulkan REQUIRED)

# Shader compilation
include(ShaderCompilation)

# Add shaders subdirectory
add_subdirectory(shaders)

# Collect source files
set(VECNA_SOURCES
    src/main.cpp
    src/Core/Application.cpp
    src/Core/FileDialog.cpp
    src/Core/GLFWContext.cpp
    src/Core/Logger.cpp
    src/Core/Window.cpp
    src/Renderer/Buffer.cpp
    src/Renderer/Pipeline.cpp
    src/Renderer/Swapchain.cpp
    src/Renderer/VulkanDevice.cpp
    src/Renderer/VulkanInstance.cpp
)

# Create executable
add_executable(${PROJECT_NAME} ${VECNA_SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    Vulkan::Vulkan
    glfw
    imgui
    vma
    pfd
)

# Apply compiler warnings
set_project_warnings(${PROJECT_NAME})

# Add shader dependency
add_dependencies(${PROJECT_NAME} compile_shaders)

# Platform-specific settings
if(WIN32)
    # Windows-specific
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(APPLE)
    # macOS with MoltenVK
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_USE_PLATFORM_MACOS_MVK
    )
elseif(UNIX)
    # Linux
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        VK_USE_PLATFORM_XLIB_KHR
    )
endif()

# Tests (optional)
option(VECNA_BUILD_TESTS "Build tests" OFF)
if(VECNA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Installation (basic)
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
)

# Print configuration summary
message(STATUS "")
message(STATUS "=== Vecna Configuration ===")
message(STATUS "Version: ${PROJECT_VERSION}")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Vulkan: ${Vulkan_VERSION}")
message(STATUS "===========================")
message(STATUS "")
