#pragma once

#include "Vecna/Renderer/Vertex.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace cgre2 {

class ShaderModule;
class SpecializationConstants;
class VulkanDevice;

/// Size of a 4x4 matrix in floats — kept for the canonical PushConstants
/// layout below, which most clients will use as-is.
inline constexpr size_t MVP_MATRIX_FLOATS = 16;

/// Default push-constants layout: MVP + model matrix (128 bytes, exactly
/// Vulkan's guaranteed minimum). Callers can declare their own struct of
/// any size up to the device's `maxPushConstantsSize`.
struct PushConstants {
    float mvp[MVP_MATRIX_FLOATS];
    float model[MVP_MATRIX_FLOATS];
};

inline constexpr uint32_t PUSH_CONSTANT_SIZE = sizeof(PushConstants);

/// Aggregate of everything a graphics pipeline needs to be created. Built
/// by the caller (typically Application). Most pipelines populate the
/// shader/vertex layout fields and leave the state knobs at their default
/// values — those produce the same rasterizer/depth/blend setup the old
/// `Pipeline(device, renderPass, shaderDir, flatShading)` ctor used.
///
/// Ownership: the pointer fields reference objects owned elsewhere (the
/// ShaderManager for shaders, the caller for layouts and specialization).
/// They must remain alive until the Pipeline ctor returns — after that
/// the data is baked into the VkPipeline.
struct PipelineCreateInfo {
    VkRenderPass                       renderPass     = VK_NULL_HANDLE;
    const ShaderModule*                vertexShader   = nullptr;
    const ShaderModule*                fragmentShader = nullptr;
    const VertexLayout*                vertexLayout   = nullptr;
    const SpecializationConstants*     specialization = nullptr;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts;
    std::vector<VkPushConstantRange>   pushConstantRanges;

    // Optional rasterizer / depth knobs. Defaults match the previous
    // hard-coded behavior so a "minimal" PipelineCreateInfo just needs
    // renderPass + the two shaders + a vertex layout.
    VkCullModeFlags     cullMode      = VK_CULL_MODE_BACK_BIT;
    VkFrontFace         frontFace     = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    VkPolygonMode       polygonMode   = VK_POLYGON_MODE_FILL;
    VkPrimitiveTopology topology      = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    bool                depthTest     = true;
    bool                depthWrite    = true;
    VkCompareOp         depthCompareOp = VK_COMPARE_OP_LESS;
};

/// RAII wrapper for VkPipeline + VkPipelineLayout. Owns nothing else —
/// shaders, descriptor set layouts and the render pass are referenced by
/// the pipeline but their lifetime is the caller's responsibility.
///
/// IMPORTANT: must be destroyed BEFORE the render pass it references.
class Pipeline {
public:
    /// Build a graphics pipeline from the given info. The info struct can
    /// be discarded as soon as this ctor returns.
    Pipeline(VulkanDevice& device, const PipelineCreateInfo& info);
    ~Pipeline();

    Pipeline(const Pipeline&) = delete;
    Pipeline& operator=(const Pipeline&) = delete;
    Pipeline(Pipeline&&) = delete;
    Pipeline& operator=(Pipeline&&) = delete;

    [[nodiscard]] VkPipeline       getPipeline()       const { return m_pipeline; }
    [[nodiscard]] VkPipelineLayout getPipelineLayout() const { return m_pipelineLayout; }

private:
    void createPipelineLayout(const PipelineCreateInfo& info);
    void createGraphicsPipeline(const PipelineCreateInfo& info);

    VulkanDevice&    m_device;
    VkPipelineLayout m_pipelineLayout = VK_NULL_HANDLE;
    VkPipeline       m_pipeline       = VK_NULL_HANDLE;
};

} // namespace cgre2
