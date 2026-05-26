#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace cgre2 {

class ShaderModule;
class DeviceContext;

/// Owns the VkDescriptorSetLayout handles + VkPushConstantRange list
/// derived from one or more ShaderModule reflections, merging bindings
/// that appear in multiple stages (e.g. a uniform buffer used by both the
/// vertex and the fragment stage produces a single binding with
/// `stageFlags = VERTEX | FRAGMENT`).
///
/// Push constant ranges with matching (offset, size) across stages are
/// likewise fused; mismatched layouts produce one range per stage and
/// Vulkan validation will catch overlap errors at pipeline-layout creation.
///
/// Lifetime: must outlive any VkPipelineLayout / VkPipeline that uses its
/// handles. Typically held by Application / a renderer-wide context.
class DescriptorLayouts {
public:
    /// Reflect-and-build from the given shaders. The vector may contain
    /// any combination of stages; nulls are silently ignored.
    DescriptorLayouts(DeviceContext& device,
                      const std::vector<const ShaderModule*>& shaders);
    ~DescriptorLayouts();

    DescriptorLayouts(const DescriptorLayouts&) = delete;
    DescriptorLayouts& operator=(const DescriptorLayouts&) = delete;
    DescriptorLayouts(DescriptorLayouts&&) = delete;
    DescriptorLayouts& operator=(DescriptorLayouts&&) = delete;

    /// One VkDescriptorSetLayout per `set` index used by the shaders,
    /// in ascending set order. Sets with no bindings are skipped — the
    /// vector is dense, not indexed by set number. For a sparse layout
    /// pattern (set 0 + set 2 used, set 1 absent), callers must inject
    /// an empty layout themselves.
    [[nodiscard]] const std::vector<VkDescriptorSetLayout>& getSetLayouts() const { return m_setLayouts; }

    /// Push constant ranges suitable for passing straight to
    /// VkPipelineLayoutCreateInfo.
    [[nodiscard]] const std::vector<VkPushConstantRange>& getPushConstantRanges() const { return m_pushConstants; }

private:
    void build(const std::vector<const ShaderModule*>& shaders);
    void destroy();

    DeviceContext&                      m_device;
    std::vector<VkDescriptorSetLayout> m_setLayouts;
    std::vector<VkPushConstantRange>   m_pushConstants;
};

} // namespace cgre2
