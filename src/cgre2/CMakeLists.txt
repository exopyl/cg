cmake_minimum_required(VERSION 3.20)

project(cgre2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(headers
    include/Vecna/Renderer.hpp
    include/Vecna/Renderer/Buffer.hpp
    include/Vecna/Renderer/DescriptorLayout.hpp
    include/Vecna/Renderer/DescriptorPool.hpp
    include/Vecna/Renderer/Pipeline.hpp
    include/Vecna/Renderer/Shader.hpp
    include/Vecna/Renderer/SpecializationConstants.hpp
    include/Vecna/Renderer/Swapchain.hpp
    include/Vecna/Renderer/Texture.hpp
    include/Vecna/Renderer/UniformBuffer.hpp
    include/Vecna/Renderer/Vertex.hpp
    include/Vecna/Renderer/VulkanDevice.hpp
    include/Vecna/Renderer/VulkanInstance.hpp
)

set(sources
    Buffer.cpp
    DescriptorLayout.cpp
    DescriptorPool.cpp
    Pipeline.cpp
    Shader.cpp
    Swapchain.cpp
    Texture.cpp
    TextureManager.cpp
    UniformBuffer.cpp
    Vertex.cpp
    VulkanDevice.cpp
    VulkanInstance.cpp
)

add_library(cgre2 STATIC ${headers} ${sources})

# Public headers under include/Vecna/Renderer/ so consumers keep including
# them as "Vecna/Renderer/X.hpp" without changes.
target_include_directories(cgre2 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Transitional dependency on vecna's Core headers (Logger.hpp, Window.hpp).
# Slated for removal in a follow-up step that introduces a renderer-local
# logger and replaces Core::Window& in Swapchain with a window-agnostic
# size source (callback or framebuffer dimensions passed in).
target_include_directories(cgre2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../vecna/include)

# Vendored stb_image (used by TextureManager). Same path the rest of the
# project uses. PRIVATE — only the .cpp needs it; the public Texture.hpp
# stays stb-free.
target_include_directories(cgre2 PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../extern)

# Public Vulkan dep — headers expose VkInstance/VkDevice/... types directly.
target_link_libraries(cgre2
    PUBLIC
        Vulkan::Vulkan
    PRIVATE
        vma
        glfw
        spirv_reflect
)
