# FetchDependencies.cmake
# Fetches external dependencies via CMake FetchContent

include(FetchContent)

# GLFW - Cross-platform windowing
message(STATUS "Fetching GLFW...")
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.3.10
    GIT_SHALLOW    TRUE
)

# Disable GLFW extras we don't need
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glfw)

# VMA - Vulkan Memory Allocator (header-only)
message(STATUS "Fetching VMA...")
FetchContent_Declare(
    vma
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG        v3.0.1
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(vma)

# Create interface library for VMA (header-only)
add_library(vma INTERFACE)
target_include_directories(vma INTERFACE ${vma_SOURCE_DIR}/include)

# Dear ImGui
message(STATUS "Fetching Dear ImGui...")
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.90.1
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(imgui)

# Create library for ImGui with Vulkan and GLFW backends
add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    glfw
    Vulkan::Vulkan
)

# Disable warnings for third-party code
if(MSVC)
    target_compile_options(imgui PRIVATE /W0)
else()
    target_compile_options(imgui PRIVATE -w)
endif()

# Portable File Dialogs - Cross-platform file dialog (header-only)
message(STATUS "Fetching portable-file-dialogs...")
FetchContent_Declare(
    portable_file_dialogs
    GIT_REPOSITORY https://github.com/samhocevar/portable-file-dialogs.git
    GIT_TAG        0.1.0
    GIT_SHALLOW    TRUE
)

# Fetch without processing CMakeLists.txt (it has old cmake_minimum_required)
FetchContent_GetProperties(portable_file_dialogs)
if(NOT portable_file_dialogs_POPULATED)
    FetchContent_Populate(portable_file_dialogs)
endif()

# Create interface library for pfd (header-only)
add_library(pfd INTERFACE)
target_include_directories(pfd INTERFACE ${portable_file_dialogs_SOURCE_DIR})

# Google Test - Testing framework (only if tests are enabled)
if(VECNA_BUILD_TESTS)
    message(STATUS "Fetching Google Test...")
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG        v1.14.0
        GIT_SHALLOW    TRUE
    )
    # Prevent overriding the parent project's compiler/linker settings on Windows
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

message(STATUS "All dependencies fetched successfully")
