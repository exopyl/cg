#include "Vecna/Renderer/Buffer.hpp"
#include "Vecna/Renderer/Pipeline.hpp"  // For Vertex struct
#include "Vecna/Renderer/VulkanDevice.hpp"
#include "Vecna/Core/Logger.hpp"

#include <vk_mem_alloc.h>

#include <cstring>
#include <stdexcept>

namespace Vecna::Renderer {

// Validate Vertex size at compile time
static_assert(sizeof(Vertex) == 36, "Vertex size must be 36 bytes (9 * float)");

Buffer::Buffer(VulkanDevice& device, VkDeviceSize size, BufferType type)
    : m_device(&device), m_size(size), m_type(type) {

    VkBufferUsageFlags usage = 0;
    bool gpuOnly = true;

    switch (type) {
        case BufferType::Vertex:
            usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            gpuOnly = true;
            break;
        case BufferType::Index:
            usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
            gpuOnly = true;
            break;
        case BufferType::Staging:
            usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
            gpuOnly = false;
            break;
    }

    createBuffer(size, usage, gpuOnly);
}

Buffer::~Buffer() {
    if (m_buffer != VK_NULL_HANDLE && m_device != nullptr) {
        vmaDestroyBuffer(m_device->getAllocator(), m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = nullptr;
        Core::Logger::info("Renderer", "Buffer destroyed");
    }
}

Buffer::Buffer(Buffer&& other) noexcept
    : m_device(other.m_device)
    , m_buffer(other.m_buffer)
    , m_allocation(other.m_allocation)
    , m_size(other.m_size)
    , m_type(other.m_type) {
    other.m_device = nullptr;
    other.m_buffer = VK_NULL_HANDLE;
    other.m_allocation = nullptr;
    other.m_size = 0;
}

Buffer& Buffer::operator=(Buffer&& other) noexcept {
    if (this != &other) {
        // Clean up existing resources
        if (m_buffer != VK_NULL_HANDLE && m_device != nullptr) {
            vmaDestroyBuffer(m_device->getAllocator(), m_buffer, m_allocation);
        }

        // Move from other
        m_device = other.m_device;
        m_buffer = other.m_buffer;
        m_allocation = other.m_allocation;
        m_size = other.m_size;
        m_type = other.m_type;

        // Reset other
        other.m_device = nullptr;
        other.m_buffer = VK_NULL_HANDLE;
        other.m_allocation = nullptr;
        other.m_size = 0;
    }
    return *this;
}

void Buffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, bool gpuOnly) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    if (gpuOnly) {
        allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    } else {
        // Staging buffer: CPU-visible, host coherent
        allocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
        allocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VkResult result = vmaCreateBuffer(
        m_device->getAllocator(),
        &bufferInfo,
        &allocInfo,
        &m_buffer,
        &m_allocation,
        nullptr
    );

    if (result != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create buffer");
        throw std::runtime_error("Failed to create buffer");
    }
}

void Buffer::upload(const void* data, VkDeviceSize size) {
    if (m_type == BufferType::Staging) {
        Core::Logger::error("Renderer", "Cannot upload to staging buffer via staging");
        throw std::runtime_error("Cannot upload to staging buffer via staging");
    }

    if (size != m_size) {
        Core::Logger::error("Renderer", "Upload size mismatch: expected " +
                          std::to_string(m_size) + ", got " + std::to_string(size));
        throw std::runtime_error("Upload size mismatch");
    }

    // Create staging buffer
    VkBufferCreateInfo stagingBufferInfo{};
    stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    stagingBufferInfo.size = size;
    stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo stagingAllocInfo{};
    stagingAllocInfo.usage = VMA_MEMORY_USAGE_CPU_ONLY;
    stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VmaAllocation stagingAllocation = nullptr;
    VmaAllocationInfo stagingAllocDetailInfo{};

    VkResult result = vmaCreateBuffer(
        m_device->getAllocator(),
        &stagingBufferInfo,
        &stagingAllocInfo,
        &stagingBuffer,
        &stagingAllocation,
        &stagingAllocDetailInfo
    );

    if (result != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create staging buffer");
        throw std::runtime_error("Failed to create staging buffer");
    }

    // Copy data to staging buffer (already mapped via VMA_ALLOCATION_CREATE_MAPPED_BIT)
    std::memcpy(stagingAllocDetailInfo.pMappedData, data, size);

    // Copy from staging to GPU buffer
    copyBuffer(stagingBuffer, m_buffer, size);

    // Cleanup staging buffer
    vmaDestroyBuffer(m_device->getAllocator(), stagingBuffer, stagingAllocation);

    Core::Logger::info("Renderer", "Buffer transfer complete (" + std::to_string(size) + " bytes)");
}

void Buffer::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    // Allocate temporary command buffer
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = m_device->getTransferCommandPool();
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    VkResult result = vkAllocateCommandBuffers(m_device->getDevice(), &allocInfo, &commandBuffer);
    if (result != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to allocate transfer command buffer");
        throw std::runtime_error("Failed to allocate transfer command buffer");
    }

    // Begin one-time command
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(m_device->getDevice(), m_device->getTransferCommandPool(), 1, &commandBuffer);
        Core::Logger::error("Renderer", "Failed to begin transfer command buffer");
        throw std::runtime_error("Failed to begin transfer command buffer");
    }

    // Copy command
    VkBufferCopy copyRegion{};
    copyRegion.srcOffset = 0;
    copyRegion.dstOffset = 0;
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    vkEndCommandBuffer(commandBuffer);

    // Submit and wait
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    result = vkQueueSubmit(m_device->getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) {
        vkFreeCommandBuffers(m_device->getDevice(), m_device->getTransferCommandPool(), 1, &commandBuffer);
        Core::Logger::error("Renderer", "Failed to submit transfer command buffer");
        throw std::runtime_error("Failed to submit transfer command buffer");
    }
    vkQueueWaitIdle(m_device->getGraphicsQueue());

    // Cleanup
    vkFreeCommandBuffers(m_device->getDevice(), m_device->getTransferCommandPool(), 1, &commandBuffer);
}

// VertexBuffer implementation

VertexBuffer::VertexBuffer(VulkanDevice& device, const std::vector<Vertex>& vertices)
    : m_buffer(device, vertices.size() * sizeof(Vertex), BufferType::Vertex)
    , m_vertexCount(static_cast<uint32_t>(vertices.size())) {

    Core::Logger::info("Renderer", "Creating vertex buffer (" +
                      std::to_string(m_buffer.getSize()) + " bytes, " +
                      std::to_string(m_vertexCount) + " vertices)");

    m_buffer.upload(vertices.data(), m_buffer.getSize());
}

// IndexBuffer implementation

IndexBuffer::IndexBuffer(VulkanDevice& device, const std::vector<uint32_t>& indices)
    : m_buffer(device, indices.size() * sizeof(uint32_t), BufferType::Index)
    , m_indexCount(static_cast<uint32_t>(indices.size())) {

    Core::Logger::info("Renderer", "Creating index buffer (" +
                      std::to_string(m_buffer.getSize()) + " bytes, " +
                      std::to_string(m_indexCount) + " indices)");

    m_buffer.upload(indices.data(), m_buffer.getSize());
}

} // namespace Vecna::Renderer
