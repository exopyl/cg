#include "Vecna/Renderer/DescriptorPool.hpp"
#include "Vecna/Renderer/VulkanDevice.hpp"
#include "Vecna/Core/Logger.hpp"

#include <array>
#include <stdexcept>

namespace cgre2 {

DescriptorPool::DescriptorPool(VulkanDevice& device, const Sizes& sizes)
    : m_device(device), m_sizes(sizes)
{
    createNewVkPool();
}

DescriptorPool::~DescriptorPool()
{
    for (VkDescriptorPool pool : m_pools) {
        if (pool != VK_NULL_HANDLE) {
            vkDestroyDescriptorPool(m_device.getDevice(), pool, nullptr);
        }
    }
}

void DescriptorPool::createNewVkPool()
{
    const std::array<VkDescriptorPoolSize, 3> poolSizes = {{
        { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         m_sizes.uniformBuffers },
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, m_sizes.combinedImageSamplers },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         m_sizes.storageBuffers },
    }};

    VkDescriptorPoolCreateInfo info{};
    info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    info.pPoolSizes    = poolSizes.data();
    info.maxSets       = m_sizes.maxSetsPerPool;
    // No FREE_DESCRIPTOR_SET_BIT: we never free individual sets, and
    // omitting the flag lets the driver pack allocations more tightly.

    VkDescriptorPool pool = VK_NULL_HANDLE;
    if (vkCreateDescriptorPool(m_device.getDevice(), &info, nullptr, &pool) != VK_SUCCESS) {
        throw std::runtime_error("DescriptorPool: vkCreateDescriptorPool failed");
    }
    m_pools.push_back(pool);
}

VkDescriptorSet DescriptorPool::allocate(VkDescriptorSetLayout layout)
{
    VkDescriptorSetAllocateInfo info{};
    info.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    info.descriptorPool     = m_pools.back();
    info.descriptorSetCount = 1;
    info.pSetLayouts        = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    VkResult res = vkAllocateDescriptorSets(m_device.getDevice(), &info, &set);
    if (res == VK_ERROR_OUT_OF_POOL_MEMORY || res == VK_ERROR_FRAGMENTED_POOL) {
        // Active pool exhausted — spin up a fresh one and retry once.
        createNewVkPool();
        info.descriptorPool = m_pools.back();
        res = vkAllocateDescriptorSets(m_device.getDevice(), &info, &set);
    }
    if (res != VK_SUCCESS) {
        throw std::runtime_error("DescriptorPool: vkAllocateDescriptorSets failed");
    }
    return set;
}

} // namespace cgre2
