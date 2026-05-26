#pragma once

#include "cgre2/DeviceContext.hpp"

#include <vulkan/vulkan.h>

#include <cstdint>
#include <memory>
#include <optional>

namespace Vecna::Vulkan {

class VulkanInstance;

/// Indices of queue families needed for the application.
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    [[nodiscard]] bool isComplete() const {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/// RAII wrapper that creates a VkDevice (graphics + present queues) from a
/// VulkanInstance + VkSurfaceKHR, and exposes the device-derived resources
/// via a contained cgre2::DeviceContext.
///
/// Pass `device.getContext()` to all cgre2 rendering classes (Pipeline,
/// Buffer, UniformBuffer, Texture, ...). The present queue and present
/// queue family — Swapchain-only consumers — stay on this class.
class VulkanDevice {
public:
    /// Create a Vulkan device from the given instance and surface.
    /// @throws std::runtime_error if no suitable GPU is found or device
    ///         creation fails.
    VulkanDevice(const VulkanInstance& instance, VkSurfaceKHR surface);
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&&) = delete;
    VulkanDevice& operator=(VulkanDevice&&) = delete;

    /// Underlying DeviceContext — pass this to cgre2 rendering classes.
    [[nodiscard]] cgre2::DeviceContext&       getContext()       { return *m_context; }
    [[nodiscard]] const cgre2::DeviceContext& getContext() const { return *m_context; }

    // Forwarding getters — same handles as `getContext().getX()`. Kept
    // for source compatibility with vecna-side code that historically
    // queried the device directly (ImGuiRenderer, ad-hoc Vulkan calls in
    // Application.cpp).
    [[nodiscard]] VkPhysicalDevice getPhysicalDevice()      const { return m_context->getPhysicalDevice(); }
    [[nodiscard]] VkDevice         getDevice()              const { return m_context->getDevice(); }
    [[nodiscard]] VkQueue          getGraphicsQueue()       const { return m_context->getGraphicsQueue(); }
    [[nodiscard]] uint32_t         getGraphicsQueueFamily() const { return m_context->getGraphicsQueueFamily(); }
    [[nodiscard]] VmaAllocator     getAllocator()           const { return m_context->getAllocator(); }
    [[nodiscard]] VkCommandPool    getTransferCommandPool() const { return m_context->getTransferCommandPool(); }

    // Present-queue specific — NOT in DeviceContext (Swapchain-only).
    [[nodiscard]] VkQueue   getPresentQueue()       const { return m_presentQueue; }
    [[nodiscard]] uint32_t  getPresentQueueFamily() const { return m_presentQueueFamily; }
    [[nodiscard]] QueueFamilyIndices getQueueFamilyIndices() const;

private:
    void pickPhysicalDevice(VkInstance instance);
    void createLogicalDevice();

    [[nodiscard]] int rateDeviceSuitability(VkPhysicalDevice device) const;
    [[nodiscard]] QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) const;

    VkSurfaceKHR     m_surface              = VK_NULL_HANDLE;  // Not owned, just stored for queries
    VkPhysicalDevice m_physicalDevice       = VK_NULL_HANDLE;
    VkDevice         m_device               = VK_NULL_HANDLE;  // OWNED here
    VkQueue          m_graphicsQueue        = VK_NULL_HANDLE;
    VkQueue          m_presentQueue         = VK_NULL_HANDLE;
    uint32_t         m_graphicsQueueFamily  = 0;
    uint32_t         m_presentQueueFamily   = 0;

    // Created once the device/queues are ready. Owns VMA allocator + the
    // transfer command pool. Destroyed BEFORE the VkDevice in our dtor.
    std::unique_ptr<cgre2::DeviceContext> m_context;
};

} // namespace Vecna::Vulkan
