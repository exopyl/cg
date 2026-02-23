#include "Vecna/Renderer/VulkanDevice.hpp"
#include "Vecna/Renderer/VulkanInstance.hpp"
#include "Vecna/Core/Logger.hpp"

#include <array>
#include <set>
#include <stdexcept>
#include <vector>

// VMA is header-only - define implementation in ONE .cpp file only
// Disable warnings for third-party code
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4100)  // unreferenced formal parameter
#pragma warning(disable : 4189)  // local variable initialized but not referenced
#pragma warning(disable : 4324)  // structure was padded due to alignment specifier
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

#if defined(_MSC_VER)
#pragma warning(pop)
#elif defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

namespace Vecna::Renderer {

// Scoring constants for GPU selection
static constexpr int DISCRETE_GPU_BONUS = 1000;

// Required device extensions
static constexpr std::array<const char*, 1> DEVICE_EXTENSIONS = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

VulkanDevice::VulkanDevice(const VulkanInstance& instance, VkSurfaceKHR surface)
    : m_surface(surface) {
    pickPhysicalDevice(instance.getInstance());
    createLogicalDevice();
    Core::Logger::info("Renderer", "Logical device created");

    createAllocator(instance.getInstance());
    Core::Logger::info("Renderer", "VMA allocator initialized");

    createTransferCommandPool();
    Core::Logger::info("Renderer", "Transfer command pool created");
}

VulkanDevice::~VulkanDevice() {
    // Wait for all GPU operations to complete before destroying resources
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    // Destroy transfer command pool before VMA allocator and device
    if (m_transferCommandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_transferCommandPool, nullptr);
        m_transferCommandPool = VK_NULL_HANDLE;
        Core::Logger::info("Renderer", "Transfer command pool destroyed");
    }

    // IMPORTANT: Destroy VMA allocator BEFORE logical device
    if (m_allocator != nullptr) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = nullptr;
    }

    if (m_device != VK_NULL_HANDLE) {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }

    Core::Logger::info("Renderer", "Vulkan device destroyed");
}

void VulkanDevice::pickPhysicalDevice(VkInstance instance) {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

    if (deviceCount == 0) {
        Core::Logger::error("Renderer", "Failed to find GPUs with Vulkan support");
        throw std::runtime_error("Failed to find GPUs with Vulkan support");
    }

    Core::Logger::info("Renderer", "Found " + std::to_string(deviceCount) + " physical device(s)");

    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

    // Score all devices and pick the best one
    int bestScore = 0;
    VkPhysicalDevice bestDevice = VK_NULL_HANDLE;

    for (uint32_t i = 0; i < deviceCount; i++) {
        VkPhysicalDevice device = devices[i];
        int score = rateDeviceSuitability(device);

        VkPhysicalDeviceProperties properties;
        vkGetPhysicalDeviceProperties(device, &properties);

        Core::Logger::info("Renderer", "GPU " + std::to_string(i) + ": " +
                          std::string(properties.deviceName) + " (score: " + std::to_string(score) + ")");

        if (score > bestScore) {
            bestScore = score;
            bestDevice = device;
        }
    }

    if (bestDevice == VK_NULL_HANDLE || bestScore == 0) {
        Core::Logger::error("Renderer", "Failed to find a suitable GPU");
        throw std::runtime_error("Failed to find a suitable GPU");
    }

    m_physicalDevice = bestDevice;

    // Log selected GPU name
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(m_physicalDevice, &properties);
    Core::Logger::info("Renderer", "Selected GPU: " + std::string(properties.deviceName));

    // Store the queue family indices
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);
    m_graphicsQueueFamily = indices.graphicsFamily.value();
    m_presentQueueFamily = indices.presentFamily.value();
}

int VulkanDevice::rateDeviceSuitability(VkPhysicalDevice device) const {
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(device, &deviceProperties);

    int score = 0;

    // Discrete GPUs are strongly preferred
    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        score += DISCRETE_GPU_BONUS;
    }

    // Maximum texture size affects quality
    score += static_cast<int>(deviceProperties.limits.maxImageDimension2D);

    // Check for required queue families - not suitable without graphics and present queues
    QueueFamilyIndices indices = findQueueFamilies(device);
    if (!indices.isComplete()) {
        return 0;
    }

    // Check for required device extensions (VK_KHR_swapchain)
    uint32_t extensionCount = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

    std::set<std::string> requiredExtensions(DEVICE_EXTENSIONS.begin(), DEVICE_EXTENSIONS.end());
    for (const auto& extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }

    if (!requiredExtensions.empty()) {
        return 0;  // Missing required extensions
    }

    // Verify swapchain adequacy (at least one format and present mode available)
    VkSurfaceCapabilitiesKHR capabilities;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr);
    if (formatCount == 0) {
        return 0;  // No surface formats available
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (presentModeCount == 0) {
        return 0;  // No present modes available
    }

    return score;
}

QueueFamilyIndices VulkanDevice::findQueueFamilies(VkPhysicalDevice device) const {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    for (uint32_t i = 0; i < queueFamilyCount; i++) {
        // Check for graphics support
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        // Check for present support
        VkBool32 presentSupport = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport == VK_TRUE) {
            indices.presentFamily = i;
        }

        // Early exit if we found both
        if (indices.isComplete()) {
            break;
        }
    }

    return indices;
}

void VulkanDevice::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    // Create queue create infos for unique queue families
    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    float queuePriority = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    // No specific device features required yet
    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.pQueueCreateInfos = queueCreateInfos.data();
    createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pEnabledFeatures = &deviceFeatures;

    // Enable required device extensions (VK_KHR_swapchain)
    createInfo.enabledExtensionCount = static_cast<uint32_t>(DEVICE_EXTENSIONS.size());
    createInfo.ppEnabledExtensionNames = DEVICE_EXTENSIONS.data();

    // Note: Device-level validation layers are deprecated since Vulkan 1.1.
    // Validation is now handled entirely at the instance level.
    // We set these to 0/nullptr for modern Vulkan implementations.
    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = nullptr;

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create logical device");
        throw std::runtime_error("Failed to create logical device");
    }

    // Retrieve the queues
    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
}

void VulkanDevice::createAllocator(VkInstance instance) {
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.physicalDevice = m_physicalDevice;
    allocatorInfo.device = m_device;
    allocatorInfo.instance = instance;

    if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create VMA allocator");
        throw std::runtime_error("Failed to create VMA allocator");
    }
}

QueueFamilyIndices VulkanDevice::getQueueFamilyIndices() const {
    // Use cached values instead of querying Vulkan again
    QueueFamilyIndices indices;
    indices.graphicsFamily = m_graphicsQueueFamily;
    indices.presentFamily = m_presentQueueFamily;
    return indices;
}

void VulkanDevice::createTransferCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;  // Short-lived command buffers
    poolInfo.queueFamilyIndex = m_graphicsQueueFamily;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_transferCommandPool) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create transfer command pool");
        throw std::runtime_error("Failed to create transfer command pool");
    }
}

} // namespace Vecna::Renderer
