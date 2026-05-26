#pragma once

#include <vulkan/vulkan.h>

#include <cstddef>

// Forward decl for VMA (full include only in the .cpp)
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

namespace cgre2 {

class DeviceContext;

/// Host-visible, persistent-mapped uniform buffer. Suitable for "per-
/// frame" data updated from the CPU each draw call (camera matrices,
/// light parameters, material factors). VMA picks the right memory
/// type — typically HOST_VISIBLE + HOST_COHERENT so the writes are
/// visible to the GPU without an explicit vkFlushMappedMemoryRanges.
///
/// For large static data (e.g. a mesh's vertex buffer) prefer the
/// staged-upload Buffer class instead — this one favors update speed
/// over GPU read bandwidth.
class UniformBuffer {
public:
    /// @param size in bytes. Caller is responsible for alignment of
    ///             individual fields per std140 / std430 conventions.
    UniformBuffer(DeviceContext& device, VkDeviceSize size);
    ~UniformBuffer();

    UniformBuffer(const UniformBuffer&) = delete;
    UniformBuffer& operator=(const UniformBuffer&) = delete;
    UniformBuffer(UniformBuffer&&) = delete;
    UniformBuffer& operator=(UniformBuffer&&) = delete;

    /// memcpy `size` bytes from `data` into the mapped region at the
    /// given `offset`. No flush needed when the memory is COHERENT
    /// (which it is for VMA's default host-visible policy).
    void update(const void* data, VkDeviceSize size, VkDeviceSize offset = 0);

    [[nodiscard]] VkBuffer     getBuffer() const { return m_buffer; }
    [[nodiscard]] VkDeviceSize getSize()   const { return m_size; }

    /// Prefilled VkDescriptorBufferInfo for vkUpdateDescriptorSets.
    [[nodiscard]] VkDescriptorBufferInfo descriptorInfo() const;

private:
    DeviceContext&  m_device;
    VkBuffer       m_buffer     = VK_NULL_HANDLE;
    VmaAllocation  m_allocation = nullptr;
    void*          m_mapped     = nullptr;
    VkDeviceSize   m_size       = 0;
};

} // namespace cgre2
