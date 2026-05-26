#include "cgre2/DeviceContext.hpp"

#include <stdexcept>

// VMA is header-only — define implementation in ONE .cpp file only.
// Disable warnings for third-party code.
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

namespace cgre2 {

DeviceContext::DeviceContext(
    VkInstance       instance,
    VkPhysicalDevice physicalDevice,
    VkDevice         device,
    VkQueue          graphicsQueue,
    uint32_t         graphicsQueueFamily)
    : m_physicalDevice(physicalDevice)
    , m_device(device)
    , m_graphicsQueue(graphicsQueue)
    , m_graphicsQueueFamily(graphicsQueueFamily)
{
    if (instance        == VK_NULL_HANDLE ||
        physicalDevice  == VK_NULL_HANDLE ||
        device          == VK_NULL_HANDLE ||
        graphicsQueue   == VK_NULL_HANDLE)
    {
        throw std::invalid_argument("DeviceContext: null Vulkan handle");
    }

    createAllocator(instance);
    createTransferCommandPool();
}

DeviceContext::~DeviceContext()
{
    // Defensive sync: ensure no transfer commands from cgre2 Buffer/Texture
    // uploads are still pending before we destroy the pool. The caller will
    // typically also vkDeviceWaitIdle() before destroying the device, but
    // double-waiting is harmless.
    if (m_device != VK_NULL_HANDLE)
        vkDeviceWaitIdle(m_device);

    // Destroy the transfer pool before VMA — pool destruction needs the
    // device, and VMA destruction must happen before the device itself
    // (the caller destroys the device after us).
    if (m_transferCommandPool != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device, m_transferCommandPool, nullptr);
        m_transferCommandPool = VK_NULL_HANDLE;
    }

    if (m_allocator != nullptr) {
        vmaDestroyAllocator(m_allocator);
        m_allocator = nullptr;
    }
}

void DeviceContext::createAllocator(VkInstance instance)
{
    VmaAllocatorCreateInfo allocatorInfo{};
    allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
    allocatorInfo.physicalDevice   = m_physicalDevice;
    allocatorInfo.device           = m_device;
    allocatorInfo.instance         = instance;

    if (vmaCreateAllocator(&allocatorInfo, &m_allocator) != VK_SUCCESS)
        throw std::runtime_error("DeviceContext: vmaCreateAllocator failed");
}

void DeviceContext::createTransferCommandPool()
{
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT; // short-lived cmdbufs
    poolInfo.queueFamilyIndex = m_graphicsQueueFamily;

    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_transferCommandPool) != VK_SUCCESS)
        throw std::runtime_error("DeviceContext: vkCreateCommandPool failed");
}

} // namespace cgre2
