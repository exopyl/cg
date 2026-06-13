#include "QtDeviceAdapter.h"

#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QVulkanInstance>

#include <stdexcept>

namespace sulina {

QtDeviceAdapter::QtDeviceAdapter(QQuickWindow *window)
{
    if (!window)
        throw std::invalid_argument("QtDeviceAdapter: null window");

    QSGRendererInterface *rif = window->rendererInterface();
    if (!rif)
        throw std::runtime_error("QtDeviceAdapter: no QSGRendererInterface");

    if (rif->graphicsApi() != QSGRendererInterface::Vulkan)
        throw std::runtime_error("QtDeviceAdapter: scene graph is not using the Vulkan RHI backend");

    QVulkanInstance *qvkInst = window->vulkanInstance();
    if (!qvkInst)
        throw std::runtime_error("QtDeviceAdapter: window has no QVulkanInstance");

    // QSGRendererInterface::getResource returns a void* that, for the
    // Vulkan resources, points at the handle (one indirection). Cast to
    // T* and dereference. Mirrors the official Qt "vulkanunderqml"
    // example.
    auto fetchPtr = [&](QSGRendererInterface::Resource r) -> void* {
        void *p = rif->getResource(window, r);
        if (!p)
            throw std::runtime_error("QtDeviceAdapter: missing Vulkan resource from Qt");
        return p;
    };

    const VkInstance       inst  = qvkInst->vkInstance();
    const VkPhysicalDevice phys  = *static_cast<VkPhysicalDevice*>(fetchPtr(QSGRendererInterface::PhysicalDeviceResource));
    const VkDevice         dev   = *static_cast<VkDevice*>        (fetchPtr(QSGRendererInterface::DeviceResource));
    const VkQueue          queue = *static_cast<VkQueue*>         (fetchPtr(QSGRendererInterface::CommandQueueResource));
    const uint32_t         qfi   = *static_cast<uint32_t*>        (fetchPtr(QSGRendererInterface::GraphicsQueueFamilyIndexResource));

    m_context = std::make_unique<cgre2::DeviceContext>(inst, phys, dev, queue, qfi);
}

QtDeviceAdapter::~QtDeviceAdapter() = default;

} // namespace sulina
