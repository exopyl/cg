cmake_minimum_required(VERSION 3.20)

project(cgre2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Window-agnostic Vulkan rendering classes. Platform bootstrap
# (VulkanInstance / VulkanDevice / Swapchain) lives in the host
# application — see vecna/src/Vulkan/ for the GLFW desktop bootstrap.
set(headers
    include/cgre2/Renderer.hpp
    include/cgre2/Buffer.hpp
    include/cgre2/DescriptorLayout.hpp
    include/cgre2/DescriptorPool.hpp
    include/cgre2/DeviceContext.hpp
    include/cgre2/Pipeline.hpp
    include/cgre2/Shader.hpp
    include/cgre2/SpecializationConstants.hpp
    include/cgre2/Texture.hpp
    include/cgre2/UniformBuffer.hpp
    include/cgre2/UniformLayouts.hpp
    include/cgre2/Vertex.hpp
    include/cgre2/Logger.hpp
    include/cgre2/Trackball.hpp
)

set(sources
    Buffer.cpp
    DescriptorLayout.cpp
    DescriptorPool.cpp
    DeviceContext.cpp
    Logger.cpp
    Pipeline.cpp
    Shader.cpp
    Texture.cpp
    TextureManager.cpp
    Trackball.cpp
    UniformBuffer.cpp
    Vertex.cpp
)

# cgre2 can be consumed by multiple top-level builds (vecna, qmlviewer).
# Vecna pre-creates `vma` (INTERFACE) and `spirv_reflect` (STATIC) targets
# via its FetchDependencies.cmake. When we are built outside vecna (e.g.
# from the qmlviewer build), we fetch those deps ourselves.
if(NOT TARGET vma)
    include(FetchContent)
    FetchContent_Declare(
        vma
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG        v3.0.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(vma)
    add_library(vma INTERFACE)
    target_include_directories(vma INTERFACE ${vma_SOURCE_DIR}/include)
endif()

if(NOT TARGET spirv_reflect)
    include(FetchContent)
    FetchContent_Declare(
        spirv_reflect
        GIT_REPOSITORY https://github.com/KhronosGroup/SPIRV-Reflect.git
        GIT_TAG        vulkan-sdk-1.3.296.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_GetProperties(spirv_reflect)
    if(NOT spirv_reflect_POPULATED)
        FetchContent_Populate(spirv_reflect)
    endif()
    add_library(spirv_reflect STATIC ${spirv_reflect_SOURCE_DIR}/spirv_reflect.c)
    target_include_directories(spirv_reflect PUBLIC ${spirv_reflect_SOURCE_DIR})
    if(MSVC)
        target_compile_options(spirv_reflect PRIVATE /W0)
    else()
        target_compile_options(spirv_reflect PRIVATE -w)
    endif()
endif()

# Vulkan needs to be findable when cgre2 is the entry point for a build.
if(NOT TARGET Vulkan::Vulkan)
    find_package(Vulkan REQUIRED)
endif()

add_library(cgre2 STATIC ${headers} ${sources})

# Public headers under include/cgre2/ — consumers include them as
# "cgre2/X.hpp".
target_include_directories(cgre2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Vendored stb_image (used by TextureManager). Same path the rest of the
# project uses. PRIVATE — only the .cpp needs it; the public Texture.hpp
# stays stb-free.
target_include_directories(cgre2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../extern)

# Public Vulkan dep — headers expose VkInstance/VkDevice/... types directly.
# No more GLFW dependency — that moved out with VulkanInstance.
target_link_libraries(cgre2
    PUBLIC
        Vulkan::Vulkan
    PRIVATE
        vma
        spirv_reflect
)
