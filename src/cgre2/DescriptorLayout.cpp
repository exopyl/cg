#include "cgre2/DescriptorLayout.hpp"
#include "cgre2/Shader.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Logger.hpp"

#include <algorithm>
#include <map>
#include <stdexcept>
#include <string>

namespace cgre2 {

namespace {

// Key for fusing descriptor bindings across stages.
struct BindingKey {
    uint32_t set;
    uint32_t binding;

    bool operator<(const BindingKey& rhs) const {
        return std::tie(set, binding) < std::tie(rhs.set, rhs.binding);
    }
};

} // namespace

DescriptorLayouts::DescriptorLayouts(DeviceContext& device,
                                     const std::vector<const ShaderModule*>& shaders)
    : m_device(device) {
    try {
        build(shaders);
    } catch (...) {
        destroy();
        throw;
    }
}

DescriptorLayouts::~DescriptorLayouts() {
    destroy();
}

void DescriptorLayouts::destroy() {
    for (auto layout : m_setLayouts) {
        if (layout != VK_NULL_HANDLE) {
            vkDestroyDescriptorSetLayout(m_device.getDevice(), layout, nullptr);
        }
    }
    m_setLayouts.clear();
    m_pushConstants.clear();
}

void DescriptorLayouts::build(const std::vector<const ShaderModule*>& shaders) {
    // Fuse descriptor bindings keyed by (set, binding). For each key, we
    // accumulate the OR of stageFlags and verify that descriptorType /
    // descriptorCount agree across stages — Vulkan would otherwise reject
    // the layout creation with a validation error that's harder to map
    // back to the offending shader.
    std::map<BindingKey, VkDescriptorSetLayoutBinding> fused;

    for (const ShaderModule* sm : shaders) {
        if (!sm) continue;
        for (const auto& b : sm->getDescriptorBindings()) {
            const BindingKey key{b.set, b.binding};
            auto it = fused.find(key);
            if (it == fused.end()) {
                VkDescriptorSetLayoutBinding vkb{};
                vkb.binding         = b.binding;
                vkb.descriptorType  = b.descriptorType;
                vkb.descriptorCount = b.descriptorCount;
                vkb.stageFlags      = b.stageFlags;
                vkb.pImmutableSamplers = nullptr;
                fused.emplace(key, vkb);
            } else {
                auto& existing = it->second;
                if (existing.descriptorType != b.descriptorType ||
                    existing.descriptorCount != b.descriptorCount) {
                    const std::string err =
                        "Descriptor binding mismatch at set=" + std::to_string(b.set) +
                        " binding=" + std::to_string(b.binding) +
                        " between shader stages (different type or count) — '" +
                        b.name + "' in " + sm->getSourcePath();
                    cgre2::Logger::error("Renderer", err);
                    throw std::runtime_error(err);
                }
                existing.stageFlags |= b.stageFlags;
            }
        }
    }

    // Group fused bindings by set index. std::map iterates in sorted
    // order, so we naturally encounter (set 0, binding *) before
    // (set 1, binding *) etc.
    std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> bySet;
    for (auto& [key, vkb] : fused) {
        bySet[key.set].push_back(vkb);
    }

    // Create one VkDescriptorSetLayout per used set. Skipping unused
    // sets keeps the vector dense — see DescriptorLayouts::getSetLayouts.
    for (const auto& [setIdx, bindings] : bySet) {
        VkDescriptorSetLayoutCreateInfo info{};
        info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = static_cast<uint32_t>(bindings.size());
        info.pBindings    = bindings.data();

        VkDescriptorSetLayout layout = VK_NULL_HANDLE;
        if (vkCreateDescriptorSetLayout(m_device.getDevice(), &info, nullptr, &layout) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create VkDescriptorSetLayout for set " +
                                     std::to_string(setIdx));
        }
        m_setLayouts.push_back(layout);
    }

    // Push constant ranges. We fuse entries that share (offset, size) by
    // OR-ing stageFlags, and emit any non-overlapping range as-is. This
    // matches the common pattern where vertex+fragment declare an
    // identical `layout(push_constant) uniform PushConstants { ... }`.
    struct PCKey {
        uint32_t offset;
        uint32_t size;
        bool operator<(const PCKey& rhs) const {
            return std::tie(offset, size) < std::tie(rhs.offset, rhs.size);
        }
    };
    std::map<PCKey, VkShaderStageFlags> fusedPC;
    for (const ShaderModule* sm : shaders) {
        if (!sm) continue;
        for (const auto& pc : sm->getPushConstants()) {
            const PCKey k{pc.offset, pc.size};
            fusedPC[k] |= pc.stageFlags;
        }
    }
    m_pushConstants.reserve(fusedPC.size());
    for (const auto& [k, stages] : fusedPC) {
        VkPushConstantRange range{};
        range.stageFlags = stages;
        range.offset     = k.offset;
        range.size       = k.size;
        m_pushConstants.push_back(range);
    }
}

} // namespace cgre2
