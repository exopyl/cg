#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

namespace cgre2 {

class VulkanDevice;

/// Allocate-only descriptor pool that grows on demand. We never free
/// individual sets — sets live as long as the pool does, and the pool
/// is reset wholesale on destruction. The pattern fits the PBR workload
/// well: a fixed scene-wide set (camera/lights) plus one set per
/// material, both allocated once at model-load time.
///
/// When a vkAllocateDescriptorSets call returns VK_ERROR_OUT_OF_POOL_
/// MEMORY or VK_ERROR_FRAGMENTED_POOL we spin up a fresh underlying
/// VkDescriptorPool with the same budget and retry. Sufficient as long
/// as the per-pool budget is sized larger than the typical material
/// count of a single asset.
class DescriptorPool {
public:
    struct Sizes {
        uint32_t uniformBuffers            = 64;
        uint32_t combinedImageSamplers     = 256;
        uint32_t storageBuffers            = 16;
        uint32_t maxSetsPerPool            = 128;
    };

    DescriptorPool(VulkanDevice& device, const Sizes& sizes = {});
    ~DescriptorPool();

    DescriptorPool(const DescriptorPool&) = delete;
    DescriptorPool& operator=(const DescriptorPool&) = delete;
    DescriptorPool(DescriptorPool&&) = delete;
    DescriptorPool& operator=(DescriptorPool&&) = delete;

    /// Allocate one set with the given layout. Spawns a new underlying
    /// VkDescriptorPool transparently if the active one is full.
    VkDescriptorSet allocate(VkDescriptorSetLayout layout);

private:
    void createNewVkPool();

    VulkanDevice&                m_device;
    Sizes                        m_sizes;
    std::vector<VkDescriptorPool> m_pools;            // grows on exhaustion
};

} // namespace cgre2
