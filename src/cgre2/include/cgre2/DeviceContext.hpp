#pragma once

#include <vulkan/vulkan.h>

#include <cstdint>

// Forward declaration for VMA (avoids dragging vk_mem_alloc.h into every
// header that holds a DeviceContext&).
struct VmaAllocator_T;
typedef VmaAllocator_T* VmaAllocator;

namespace cgre2 {

/// Non-owning view of an externally-created Vulkan device, with an owned
/// VMA allocator and an owned transfer command pool.
///
/// DeviceContext does NOT own:
///   - VkInstance (provided at construction, used only to seed VMA)
///   - VkPhysicalDevice
///   - VkDevice
///   - VkQueue
/// The caller MUST guarantee those handles outlive any DeviceContext built
/// on top of them.
///
/// DeviceContext DOES own:
///   - VmaAllocator
///   - VkCommandPool (used by Buffer/Texture for staging uploads)
///
/// Two construction sites are expected in this codebase:
///
///   1. vecna::VulkanDevice (vecna/src/Vulkan/VulkanDevice.cpp) picks a
///      physical device, creates a logical device + graphics/present
///      queues, then constructs a DeviceContext over the resulting handles.
///
///   2. qmlviewer's QtDeviceAdapter (qmlviewer/src/QtDeviceAdapter.cpp)
///      pulls the handles already created by Qt's QRhi from
///      QSGRendererInterface, then constructs a DeviceContext over them.
///
/// All cgre2 rendering classes (Pipeline, Buffer, UniformBuffer, Texture,
/// Shader, DescriptorLayout/Pool) hold a reference — or a non-owning
/// pointer, for movable types like Buffer — to a DeviceContext and never
/// see the platform-specific bootstrap code.
class DeviceContext {
public:
    /// Wrap an externally-created Vulkan device, then create a VMA
    /// allocator and a transfer command pool on top.
    ///
    /// @param instance              VkInstance from which `physicalDevice`
    ///                              was selected. Used only inside the
    ///                              ctor to seed VMA; not stored.
    /// @param physicalDevice        Physical device backing `device`.
    /// @param device                Logical device. NOT destroyed by us.
    /// @param graphicsQueue         Queue from `graphicsQueueFamily`.
    /// @param graphicsQueueFamily   Index of the queue family. Used to
    ///                              create the transfer command pool.
    /// @throws std::invalid_argument if any handle is VK_NULL_HANDLE.
    /// @throws std::runtime_error on VMA or pool creation failure.
    DeviceContext(
        VkInstance       instance,
        VkPhysicalDevice physicalDevice,
        VkDevice         device,
        VkQueue          graphicsQueue,
        uint32_t         graphicsQueueFamily);

    ~DeviceContext();

    // Non-copyable, non-movable: RAII for VMA + pool. Holders that need
    // to be movable (e.g. cgre2::Buffer) hold a DeviceContext* instead of
    // a DeviceContext&.
    DeviceContext(const DeviceContext&)            = delete;
    DeviceContext& operator=(const DeviceContext&) = delete;
    DeviceContext(DeviceContext&&)                 = delete;
    DeviceContext& operator=(DeviceContext&&)      = delete;

    [[nodiscard]] VkDevice         getDevice()              const { return m_device; }
    [[nodiscard]] VkPhysicalDevice getPhysicalDevice()      const { return m_physicalDevice; }
    [[nodiscard]] VkQueue          getGraphicsQueue()       const { return m_graphicsQueue; }
    [[nodiscard]] uint32_t         getGraphicsQueueFamily() const { return m_graphicsQueueFamily; }

    /// VMA allocator. Owned — do NOT destroy externally.
    [[nodiscard]] VmaAllocator     getAllocator()           const { return m_allocator; }

    /// Transfer command pool. Owned — do NOT destroy externally.
    [[nodiscard]] VkCommandPool    getTransferCommandPool() const { return m_transferCommandPool; }

private:
    void createAllocator(VkInstance instance);
    void createTransferCommandPool();

    VkPhysicalDevice m_physicalDevice      = VK_NULL_HANDLE;
    VkDevice         m_device              = VK_NULL_HANDLE;
    VkQueue          m_graphicsQueue       = VK_NULL_HANDLE;
    uint32_t         m_graphicsQueueFamily = 0;
    VmaAllocator     m_allocator           = nullptr;
    VkCommandPool    m_transferCommandPool = VK_NULL_HANDLE;
};

} // namespace cgre2
