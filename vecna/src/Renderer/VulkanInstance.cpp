#include "Vecna/Renderer/VulkanInstance.hpp"
#include "Vecna/Core/Logger.hpp"

#include <GLFW/glfw3.h>

#include <array>
#include <cstring>
#include <stdexcept>

namespace Vecna::Renderer {

// Validation layer configuration
#ifdef NDEBUG
constexpr bool ENABLE_VALIDATION_LAYERS = false;
#else
constexpr bool ENABLE_VALIDATION_LAYERS = true;
#endif

// Static constexpr array to avoid dynamic allocation at startup
static constexpr std::array<const char*, 1> VALIDATION_LAYERS = {
    "VK_LAYER_KHRONOS_validation"
};

// Extension function proxies (not loaded by default)
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDebugUtilsMessengerEXT* pDebugMessenger) {

    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks* pAllocator) {

    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

VulkanInstance::VulkanInstance() {
    createInstance();
    setupDebugMessenger();
}

VulkanInstance::~VulkanInstance() {
    // Destroy debug messenger BEFORE instance (reverse order of creation)
    destroyDebugMessenger();

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        Core::Logger::info("Renderer", "Vulkan instance destroyed");
    }
}

void VulkanInstance::createInstance() {
    // Check validation layer support if enabled
    if (ENABLE_VALIDATION_LAYERS) {
        if (!checkValidationLayerSupport()) {
            Core::Logger::warn("Renderer", "Validation layers requested but not available");
            m_validationEnabled = false;
        } else {
            m_validationEnabled = true;
            Core::Logger::info("Renderer", "Validation layers enabled");
        }
    }

    // Application info
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Vecna";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Vecna Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    // Instance create info
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Extensions
    auto extensions = getRequiredExtensions();
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();

    // Validation layers and debug messenger for instance creation
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (m_validationEnabled) {
        createInfo.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
        createInfo.ppEnabledLayerNames = VALIDATION_LAYERS.data();

        // Enable debug messenger during instance creation/destruction
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.pNext = &debugCreateInfo;
    } else {
        createInfo.enabledLayerCount = 0;
        createInfo.pNext = nullptr;
    }
    // Note: debugCreateInfo must stay in scope until vkCreateInstance returns

    // Create instance
    VkResult result = vkCreateInstance(&createInfo, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create Vulkan instance");
        throw std::runtime_error("Failed to create Vulkan instance");
    }

    Core::Logger::info("Renderer", "Vulkan instance created");
}

void VulkanInstance::setupDebugMessenger() {
    if (!m_validationEnabled) {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT createInfo{};
    populateDebugMessengerCreateInfo(createInfo);

    VkResult result = CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger);
    if (result != VK_SUCCESS) {
        Core::Logger::warn("Renderer", "Failed to create debug messenger");
        // Non-fatal: continue without debug messenger
    } else {
        Core::Logger::debug("Renderer", "Debug messenger created");
    }
}

void VulkanInstance::destroyDebugMessenger() {
    if (m_validationEnabled && m_debugMessenger != VK_NULL_HANDLE) {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        m_debugMessenger = VK_NULL_HANDLE;
        Core::Logger::debug("Renderer", "Debug messenger destroyed");
    }
}

bool VulkanInstance::checkValidationLayerSupport() const {
    uint32_t layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const char* layerName : VALIDATION_LAYERS) {
        bool layerFound = false;
        for (const auto& layerProperties : availableLayers) {
            if (std::strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) {
            return false;
        }
    }

    return true;
}

std::vector<const char*> VulkanInstance::getRequiredExtensions() const {
    // Get GLFW required extensions
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    // Check if GLFW supports Vulkan (returns nullptr if not)
    if (glfwExtensions == nullptr) {
        Core::Logger::error("Renderer", "GLFW does not support Vulkan or is not initialized");
        throw std::runtime_error("GLFW does not support Vulkan");
    }

    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

    // Add debug utils extension if validation is enabled
    if (m_validationEnabled) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    // Log enabled extensions
    Core::Logger::debug("Renderer", "Vulkan extensions enabled: " + std::to_string(extensions.size()));

    return extensions;
}

void VulkanInstance::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
    createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity =
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType =
        VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData = nullptr;
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanInstance::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    [[maybe_unused]] VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    [[maybe_unused]] void* pUserData) {

    // Map Vulkan severity to Logger level
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        Core::Logger::error("Vulkan", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        Core::Logger::warn("Vulkan", pCallbackData->pMessage);
    } else {
        Core::Logger::debug("Vulkan", pCallbackData->pMessage);
    }

    return VK_FALSE;
}

} // namespace Vecna::Renderer
