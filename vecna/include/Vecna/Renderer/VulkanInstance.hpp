#pragma once

#include <vulkan/vulkan.h>

#include <vector>

namespace Vecna::Renderer {

/// RAII wrapper for VkInstance with optional validation layers.
/// Validation layers are enabled in Debug builds, disabled in Release.
class VulkanInstance {
public:
    /// Create a Vulkan instance with required extensions for GLFW.
    /// @throws std::runtime_error if instance creation fails.
    VulkanInstance();
    ~VulkanInstance();

    // Non-copyable, non-movable
    VulkanInstance(const VulkanInstance&) = delete;
    VulkanInstance& operator=(const VulkanInstance&) = delete;
    VulkanInstance(VulkanInstance&&) = delete;
    VulkanInstance& operator=(VulkanInstance&&) = delete;

    /// Get the raw Vulkan instance handle.
    /// @note The caller must NOT destroy this handle - it is owned by VulkanInstance.
    [[nodiscard]] VkInstance getInstance() const { return m_instance; }

    /// Check if validation layers are enabled.
    [[nodiscard]] bool isValidationEnabled() const { return m_validationEnabled; }

private:
    void createInstance();
    void setupDebugMessenger();
    void destroyDebugMessenger();

    [[nodiscard]] bool checkValidationLayerSupport() const;
    [[nodiscard]] std::vector<const char*> getRequiredExtensions() const;

    static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    bool m_validationEnabled = false;
};

} // namespace Vecna::Renderer
