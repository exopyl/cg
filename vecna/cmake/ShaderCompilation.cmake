# ShaderCompilation.cmake
# Configures shader compilation from GLSL to SPIR-V using glslc

# Find glslc from Vulkan SDK
find_program(GLSLC glslc
    HINTS
        $ENV{VULKAN_SDK}/Bin
        $ENV{VULKAN_SDK}/bin
        $ENV{VK_SDK_PATH}/Bin
        $ENV{VK_SDK_PATH}/bin
)

if(NOT GLSLC)
    message(FATAL_ERROR "glslc not found. Please install Vulkan SDK and ensure glslc is in PATH.")
endif()

message(STATUS "Found glslc: ${GLSLC}")

# Vulkan push constant size limits
# Vulkan spec guarantees minimum 128 bytes for push constants
# Our shaders use mat4 (64 bytes) which is well within limits
set(VECNA_PUSH_CONSTANT_SIZE 64 CACHE INTERNAL "Push constant size in bytes (mat4 = 64)")
set(VULKAN_MIN_PUSH_CONSTANT_SIZE 128 CACHE INTERNAL "Vulkan minimum guaranteed push constant size")

if(VECNA_PUSH_CONSTANT_SIZE GREATER VULKAN_MIN_PUSH_CONSTANT_SIZE)
    message(FATAL_ERROR "Push constant size (${VECNA_PUSH_CONSTANT_SIZE}) exceeds Vulkan minimum guarantee (${VULKAN_MIN_PUSH_CONSTANT_SIZE})")
endif()

message(STATUS "Push constant size: ${VECNA_PUSH_CONSTANT_SIZE} bytes (Vulkan min: ${VULKAN_MIN_PUSH_CONSTANT_SIZE})")
