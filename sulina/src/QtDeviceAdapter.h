#pragma once

#include "cgre2/DeviceContext.hpp"

#include <memory>

class QQuickWindow;

namespace sulina {

/// Adapter that pulls Vulkan handles from a QQuickWindow whose scene
/// graph was initialized with QSGRendererInterface::Vulkan, and exposes
/// them via a cgre2::DeviceContext so cgre2 rendering classes (Pipeline,
/// Buffer, ...) can be built on top of Qt's device.
///
/// The window's scene graph must be initialized before this is constructed.
/// Construct from the `QQuickWindow::sceneGraphInitialized` signal handler,
/// not from `componentComplete()`.
///
/// All Vulkan handles are NON-OWNED — Qt destroys them when the scene
/// graph is invalidated. Therefore this adapter MUST be destroyed via
/// the `QQuickWindow::sceneGraphInvalidated` signal, before Qt tears
/// down the device.
class QtDeviceAdapter {
public:
    explicit QtDeviceAdapter(QQuickWindow *window);
    ~QtDeviceAdapter();

    QtDeviceAdapter(const QtDeviceAdapter&) = delete;
    QtDeviceAdapter& operator=(const QtDeviceAdapter&) = delete;

    cgre2::DeviceContext&       context()       { return *m_context; }
    const cgre2::DeviceContext& context() const { return *m_context; }

private:
    std::unique_ptr<cgre2::DeviceContext> m_context;
};

} // namespace sulina
