#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

// Forward declaration for VMA
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

namespace Vecna::Renderer {

class VulkanDevice;
struct Vertex;

/// Type of buffer for GPU memory allocation.
enum class BufferType {
    Vertex,   ///< GPU-only vertex buffer (VK_BUFFER_USAGE_VERTEX_BUFFER_BIT)
    Index,    ///< GPU-only index buffer (VK_BUFFER_USAGE_INDEX_BUFFER_BIT)
    Staging   ///< CPU-visible staging buffer (VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
};

/// RAII wrapper for VkBuffer with VMA allocation.
/// Handles buffer creation, memory allocation, and cleanup.
/// Supports staging buffer transfers for GPU-only buffers.
///
/// @note IMPORTANT: Buffers must be destroyed BEFORE VulkanDevice.
/// They require VMA allocator for cleanup.
class Buffer {
public:
    /// Create a buffer of the specified type and size.
    /// @param device The Vulkan device for allocation.
    /// @param size Size in bytes.
    /// @param type Buffer type (Vertex, Index, or Staging).
    /// @throws std::runtime_error if buffer creation fails.
    Buffer(VulkanDevice& device, VkDeviceSize size, BufferType type);
    ~Buffer();

    // Non-copyable
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;

    // Movable
    Buffer(Buffer&& other) noexcept;
    Buffer& operator=(Buffer&& other) noexcept;

    /// Upload data to the buffer via staging buffer.
    /// Only valid for Vertex and Index buffer types.
    /// @param data Pointer to data to upload.
    /// @param size Size in bytes (must match buffer size).
    /// @throws std::runtime_error if upload fails or buffer type is Staging.
    void upload(const void* data, VkDeviceSize size);

    /// Get the Vulkan buffer handle.
    [[nodiscard]] VkBuffer getBuffer() const { return m_buffer; }

    /// Get the buffer size in bytes.
    [[nodiscard]] VkDeviceSize getSize() const { return m_size; }

    /// Get the buffer type.
    [[nodiscard]] BufferType getType() const { return m_type; }

    /// Get the VMA allocation (for debugging/introspection).
    [[nodiscard]] VmaAllocation getAllocation() const { return m_allocation; }

private:
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool gpuOnly);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);

    VulkanDevice* m_device = nullptr;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VmaAllocation m_allocation = nullptr;
    VkDeviceSize m_size = 0;
    BufferType m_type = BufferType::Vertex;
};

/// Specialized vertex buffer for mesh rendering.
/// Wraps Buffer with vertex-specific functionality.
class VertexBuffer {
public:
    /// Create a vertex buffer from vertex data.
    /// @param device The Vulkan device for allocation.
    /// @param vertices Vector of vertices to upload.
    /// @throws std::runtime_error if creation fails.
    VertexBuffer(VulkanDevice& device, const std::vector<Vertex>& vertices);

    // Use default move operations (Buffer is movable)
    VertexBuffer(VertexBuffer&&) noexcept = default;
    VertexBuffer& operator=(VertexBuffer&&) noexcept = default;

    // Non-copyable
    VertexBuffer(const VertexBuffer&) = delete;
    VertexBuffer& operator=(const VertexBuffer&) = delete;

    /// Get the Vulkan buffer handle.
    [[nodiscard]] VkBuffer getBuffer() const { return m_buffer.getBuffer(); }

    /// Get the number of vertices in the buffer.
    [[nodiscard]] uint32_t getVertexCount() const { return m_vertexCount; }

    /// Get the buffer size in bytes.
    [[nodiscard]] VkDeviceSize getSize() const { return m_buffer.getSize(); }

private:
    Buffer m_buffer;
    uint32_t m_vertexCount = 0;
};

/// Specialized index buffer for indexed mesh rendering.
/// Wraps Buffer with index-specific functionality.
class IndexBuffer {
public:
    /// Create an index buffer from index data.
    /// @param device The Vulkan device for allocation.
    /// @param indices Vector of uint32_t indices to upload.
    /// @throws std::runtime_error if creation fails.
    IndexBuffer(VulkanDevice& device, const std::vector<uint32_t>& indices);

    // Use default move operations (Buffer is movable)
    IndexBuffer(IndexBuffer&&) noexcept = default;
    IndexBuffer& operator=(IndexBuffer&&) noexcept = default;

    // Non-copyable
    IndexBuffer(const IndexBuffer&) = delete;
    IndexBuffer& operator=(const IndexBuffer&) = delete;

    /// Get the Vulkan buffer handle.
    [[nodiscard]] VkBuffer getBuffer() const { return m_buffer.getBuffer(); }

    /// Get the number of indices in the buffer.
    [[nodiscard]] uint32_t getIndexCount() const { return m_indexCount; }

    /// Get the buffer size in bytes.
    [[nodiscard]] VkDeviceSize getSize() const { return m_buffer.getSize(); }

private:
    Buffer m_buffer;
    uint32_t m_indexCount = 0;
};

} // namespace Vecna::Renderer
