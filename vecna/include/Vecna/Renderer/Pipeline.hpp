#pragma once

#include <vulkan/vulkan.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace Vecna::Renderer {

class VulkanDevice;

/// Vertex structure with interleaved position, normal, and color.
/// Layout matches GLSL shader inputs at locations 0, 1, 2.
struct Vertex {
    float position[3];
    float normal[3];
    float color[3];

    /// Get the binding description for this vertex format.
    static VkVertexInputBindingDescription getBindingDescription();

    /// Get the attribute descriptions for position, normal, and color.
    static std::array<VkVertexInputAttributeDescription, 3> getAttributeDescriptions();
};

/// Size of a 4x4 matrix in floats.
inline constexpr size_t MVP_MATRIX_FLOATS = 16;

/// Push constants structure for MVP matrix.
/// Size: 64 bytes (mat4), well within Vulkan's 128 byte minimum guarantee.
struct PushConstants {
    float mvp[MVP_MATRIX_FLOATS];  // 4x4 matrix
};

/// Size of push constants in bytes.
inline constexpr uint32_t PUSH_CONSTANT_SIZE = sizeof(PushConstants);

/// RAII wrapper for VkPipeline and VkPipelineLayout.
/// Creates a graphics pipeline configured for 3D rendering with:
/// - Interleaved vertex format (position, normal, color)
/// - Push constants for MVP matrix
/// - Dynamic viewport and scissor states
/// - Back-face culling, polygon fill mode
/// - Depth testing disabled (prepared for Story 2-4)
///
/// @note IMPORTANT: Pipeline must be destroyed BEFORE Swapchain.
/// The pipeline references the render pass from Swapchain.
class Pipeline {
public:
    /// Create a graphics pipeline for the given device and render pass.
    /// @param device The Vulkan device to use.
    /// @param renderPass The render pass this pipeline will be used with.
    /// @param shaderDir Directory containing compiled SPIR-V shaders.
    /// @throws std::runtime_error if pipeline creation fails or shaders not found.
    Pipeline(VulkanDevice& device, VkRenderPass renderPass, const std::string& shaderDir);
    ~Pipeline();

    // Non-copyable, non-movable
    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;

    /// Get the graphics pipeline handle.
    [[nodiscard]] VkPipeline getPipeline() const { return m_pipeline; }

    /// Get the pipeline layout handle.
    [[nodiscard]] VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }

private:
    void createPipelineLayout();
    void createGraphicsPipeline(VkRenderPass renderPass, const std::string& shaderDir);

    /// Read SPIR-V shader file. Returns uint32_t vector for proper alignment.
    [[nodiscard]] static std::vector<uint32_t> readShaderFile(const std::string& filename);
    [[nodiscard]] VkShaderModule createShaderModule(const std::vector<uint32_t>& code) const;

    VulkanDevice& m_device;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline m_pipeline = VK_NULL_HANDLE;
};

} // namespace Vecna::Renderer
