#include "Vecna/Renderer/UniformBuffer.hpp"
#include "Vecna/Renderer/VulkanDevice.hpp"

#include <vk_mem_alloc.h>

#include <cstring>
#include <stdexcept>

namespace cgre2 {

UniformBuffer::UniformBuffer(VulkanDevice& device, VkDeviceSize size)
    : m_device(device), m_size(size)
{
    if (size == 0) {
        throw std::runtime_error("UniformBuffer: size cannot be zero");
    }

    VkBufferCreateInfo bc{};
    bc.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bc.size        = size;
    bc.usage       = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bc.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo ac{};
    // CPU_TO_GPU on x86 desktops picks DEVICE_LOCAL | HOST_VISIBLE | HOST_
    // COHERENT (the "BAR" memory). Falls back to plain HOST_VISIBLE +
    // HOST_COHERENT elsewhere. Mapped state persists for the lifetime of
    // the allocation so update() is a pure memcpy.
    ac.usage = VMA_MEMORY_USAGE_CPU_TO_GPU;
    ac.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VmaAllocationInfo info{};
    if (vmaCreateBuffer(m_device.getAllocator(), &bc, &ac,
                        &m_buffer, &m_allocation, &info) != VK_SUCCESS) {
        throw std::runtime_error("UniformBuffer: vmaCreateBuffer failed");
    }
    m_mapped = info.pMappedData;
    if (!m_mapped) {
        // Defensive: if the memory type ended up non-mappable for any
        // reason, the contract of this class is broken. Better fail
        // loud here than silently corrupt later.
        throw std::runtime_error("UniformBuffer: allocation is not host-mapped");
    }
}

UniformBuffer::~UniformBuffer()
{
    if (m_buffer != VK_NULL_HANDLE) {
        vmaDestroyBuffer(m_device.getAllocator(), m_buffer, m_allocation);
    }
}

void UniformBuffer::update(const void* data, VkDeviceSize size, VkDeviceSize offset)
{
    if (offset + size > m_size) {
        throw std::runtime_error("UniformBuffer::update: offset+size exceeds buffer size");
    }
    std::memcpy(static_cast<char*>(m_mapped) + offset, data, static_cast<size_t>(size));
    // No flush needed: VMA_MEMORY_USAGE_CPU_TO_GPU picks HOST_COHERENT.
}

VkDescriptorBufferInfo UniformBuffer::descriptorInfo() const
{
    VkDescriptorBufferInfo info{};
    info.buffer = m_buffer;
    info.offset = 0;
    info.range  = m_size;
    return info;
}

} // namespace cgre2
