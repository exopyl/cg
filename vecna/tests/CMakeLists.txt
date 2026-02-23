# tests/CMakeLists.txt
# Test configuration for Vecna using Google Test

include(CTest)
include(GoogleTest)

# Collect test sources
set(TEST_SOURCES
    Core/LoggerTest.cpp
    Core/WindowConfigTest.cpp
    Renderer/BufferTest.cpp
    Renderer/PipelineTest.cpp
    Renderer/QueueFamilyIndicesTest.cpp
)

# Create test executable
add_executable(vecna_tests ${TEST_SOURCES})

# Include directories
target_include_directories(vecna_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link against Google Test and project dependencies
target_link_libraries(vecna_tests PRIVATE
    GTest::gtest_main
    GTest::gmock
    Vulkan::Vulkan
    glfw
)

# Add source files that tests need (without main.cpp)
target_sources(vecna_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/src/Core/Logger.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/Window.cpp
    ${CMAKE_SOURCE_DIR}/src/Core/GLFWContext.cpp
    ${CMAKE_SOURCE_DIR}/src/Renderer/Pipeline.cpp
)

# Apply same compile definitions as main project
if(WIN32)
    target_compile_definitions(vecna_tests PRIVATE
        VK_USE_PLATFORM_WIN32_KHR
        NOMINMAX
        WIN32_LEAN_AND_MEAN
    )
elseif(APPLE)
    target_compile_definitions(vecna_tests PRIVATE
        VK_USE_PLATFORM_MACOS_MVK
    )
elseif(UNIX)
    target_compile_definitions(vecna_tests PRIVATE
        VK_USE_PLATFORM_XLIB_KHR
    )
endif()

# Register tests with CTest
gtest_discover_tests(vecna_tests)
