#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>
#include <vector>

// Forward declaration for VMA
struct VmaAllocation_T;
typedef VmaAllocation_T* VmaAllocation;

namespace Vecna::Core {
class Window;
} // namespace Vecna::Core

namespace Vecna::Renderer {

class VulkanDevice;

/// Details about swapchain support for a physical device.
struct SwapchainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities{};
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

/// RAII wrapper for VkSwapchainKHR with image views and framebuffers.
/// Handles swapchain creation, recreation on resize, and synchronization.
/// @note IMPORTANT: Swapchain must be destroyed BEFORE VulkanDevice.
/// The destructor calls vkDeviceWaitIdle and uses the device handle.
class Swapchain {
public:
    /// Maximum number of frames that can be processed concurrently.
    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;

    /// Create a swapchain for the given device and surface.
    /// @param device The Vulkan device to use.
    /// @param surface The Vulkan surface for presentation.
    /// @param window The window for dimensions.
    /// @throws std::runtime_error if swapchain creation fails.
    Swapchain(VulkanDevice& device, VkSurfaceKHR surface, Core::Window& window);
    ~Swapchain();

    // Non-copyable, non-movable
    Swapchain(const Swapchain&) = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain(Swapchain&&) = delete;
    Swapchain& operator=(Swapchain&&) = delete;

    /// Get the swapchain handle.
    [[nodiscard]] VkSwapchainKHR getSwapchain() const { return m_swapchain; }

    /// Get the swapchain image format.
    [[nodiscard]] VkFormat getImageFormat() const { return m_imageFormat; }

    /// Get the swapchain extent (dimensions).
    [[nodiscard]] VkExtent2D getExtent() const { return m_extent; }

    /// Get the render pass.
    [[nodiscard]] VkRenderPass getRenderPass() const { return m_renderPass; }

    /// Get the depth buffer format.
    [[nodiscard]] VkFormat getDepthFormat() const { return m_depthFormat; }

    /// Get the framebuffer for a specific image index.
    /// @throws std::out_of_range if imageIndex is invalid.
    [[nodiscard]] VkFramebuffer getFramebuffer(uint32_t imageIndex) const { return m_framebuffers.at(imageIndex); }

    /// Get the number of swapchain images.
    [[nodiscard]] uint32_t getImageCount() const { return static_cast<uint32_t>(m_images.size()); }

    /// Acquire the next image from the swapchain.
    /// @param imageIndex Output parameter for the acquired image index.
    /// @return VK_SUCCESS, VK_ERROR_OUT_OF_DATE_KHR, or VK_SUBOPTIMAL_KHR.
    VkResult acquireNextImage(uint32_t* imageIndex);

    /// Submit a command buffer and present the image.
    /// @param commandBuffer The command buffer to submit.
    /// @param imageIndex The image index to present.
    /// @return VK_SUCCESS, VK_ERROR_OUT_OF_DATE_KHR, or VK_SUBOPTIMAL_KHR.
    VkResult submitAndPresent(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    /// Recreate the swapchain (e.g., after window resize).
    void recreate();

    /// Get the current frame index (for synchronization).
    [[nodiscard]] uint32_t getCurrentFrame() const { return m_currentFrame; }

    /// Get the command buffer for the current frame.
    [[nodiscard]] VkCommandBuffer getCommandBuffer() const { return m_commandBuffers[m_currentFrame]; }

    /// Query swapchain support details for a physical device.
    static SwapchainSupportDetails querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

private:
    void createSwapchain();
    void createImageViews();
    void createDepthResources();
    void createRenderPass();
    void createFramebuffers();
    void createSyncObjects();
    void createCommandPool();
    void createCommandBuffers();

    void cleanupSwapchain();

    /// Find a supported depth format for the physical device.
    [[nodiscard]] VkFormat findDepthFormat() const;

    [[nodiscard]] VkSurfaceFormatKHR chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR>& availableFormats) const;
    [[nodiscard]] VkPresentModeKHR chooseSwapPresentMode(
        const std::vector<VkPresentModeKHR>& availableModes) const;
    [[nodiscard]] VkExtent2D chooseSwapExtent(
        const VkSurfaceCapabilitiesKHR& capabilities) const;

    VulkanDevice& m_device;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    Core::Window& m_window;

    VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
    VkFormat m_imageFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D m_extent{};

    std::vector<VkImage> m_images;
    std::vector<VkImageView> m_imageViews;

    VkRenderPass m_renderPass = VK_NULL_HANDLE;
    std::vector<VkFramebuffer> m_framebuffers;

    // Depth buffer resources
    VkImage m_depthImage = VK_NULL_HANDLE;
    VmaAllocation m_depthImageAllocation = nullptr;
    VkImageView m_depthImageView = VK_NULL_HANDLE;
    VkFormat m_depthFormat = VK_FORMAT_UNDEFINED;

    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_commandBuffers;

    // Synchronization objects (per frame in flight)
    std::vector<VkSemaphore> m_imageAvailableSemaphores;
    std::vector<VkSemaphore> m_renderFinishedSemaphores;
    std::vector<VkFence> m_inFlightFences;
    uint32_t m_currentFrame = 0;
};

} // namespace Vecna::Renderer
