#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <optional>

// Forward declaration for VMA
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;

namespace Vecna::Renderer {

class VulkanInstance;

/// Indices of queue families needed for the application.
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    [[nodiscard]] bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/// RAII wrapper for VkDevice with physical device selection and VMA allocator.
/// Selects the most suitable GPU (discrete preferred) and creates a logical device.
class VulkanDevice {
public:
    /// Create a Vulkan device from the given instance and surface.
    /// @param instance The Vulkan instance to use for device enumeration.
    /// @param surface The Vulkan surface for presentation support check.
    /// @throws std::runtime_error if no suitable GPU is found or device creation fails.
    VulkanDevice(const VulkanInstance& instance, VkSurfaceKHR surface);
    ~VulkanDevice();

    // Non-copyable, non-movable
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;

    /// Get the physical device handle.
    [[nodiscard]] VkPhysicalDevice getPhysicalDevice() const { return m_physicalDevice; }

    /// Get the logical device handle.
    [[nodiscard]] VkDevice getDevice() const { return m_device; }

    /// Get the graphics queue.
    [[nodiscard]] VkQueue getGraphicsQueue() const { return m_graphicsQueue; }

    /// Get the present queue.
    [[nodiscard]] VkQueue getPresentQueue() const { return m_presentQueue; }

    /// Get the graphics queue family index.
    [[nodiscard]] uint32_t getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }

    /// Get the present queue family index.
    [[nodiscard]] uint32_t getPresentQueueFamily() const { return m_presentQueueFamily; }

    /// Get the VMA allocator.
    [[nodiscard]] VmaAllocator getAllocator() const { return m_allocator; }

    /// Get the command pool for transfer operations.
    [[nodiscard]] VkCommandPool getTransferCommandPool() const { return m_transferCommandPool; }

    /// Get the queue family indices.
    [[nodiscard]] QueueFamilyIndices getQueueFamilyIndices() const;

private:
    void pickPhysicalDevice(VkInstance instance);
    void createLogicalDevice();
    void createAllocator(VkInstance instance);
    void createTransferCommandPool();

    [[nodiscard]] int rateDeviceSuitability(VkPhysicalDevice device) const;
    [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

    VkSurfaceKHR m_surface = VK_NULL_HANDLE;  // Not owned, just stored for queries
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;
    uint32_t m_graphicsQueueFamily = 0;
    uint32_t m_presentQueueFamily = 0;
    VmaAllocator m_allocator = nullptr;
    VkCommandPool m_transferCommandPool = VK_NULL_HANDLE;
};

} // namespace Vecna::Renderer
