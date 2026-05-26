#pragma once

// Renderer module aggregation header
//
// Window-agnostic rendering classes. VulkanInstance/VulkanDevice/Swapchain
// are platform bootstrap and live in vecna/include/Vecna/Vulkan/ (or any
// other host application's bootstrap layer, e.g. qmlviewer's
// QtDeviceAdapter).
#include "cgre2/Buffer.hpp"
#include "cgre2/DeviceContext.hpp"
#include "cgre2/Pipeline.hpp"
