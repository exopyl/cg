#include "Vecna/Renderer/Swapchain.hpp"
#include "Vecna/Renderer/VulkanDevice.hpp"
#include "Vecna/Core/Logger.hpp"
#include "Vecna/Core/Window.hpp"

#include <vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace Vecna::Renderer {

Swapchain::Swapchain(VulkanDevice& device, VkSurfaceKHR surface, Core::Window& window)
    : m_device(device)
    , m_surface(surface)
    , m_window(window) {
    createSwapchain();
    createImageViews();
    createDepthResources();
    createRenderPass();
    createFramebuffers();
    createCommandPool();
    createCommandBuffers();
    createSyncObjects();
}

Swapchain::~Swapchain() {
    // Wait for device to be idle before cleanup
    vkDeviceWaitIdle(m_device.getDevice());

    cleanupSwapchain();

    // Destroy sync objects
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(m_device.getDevice(), m_imageAvailableSemaphores[i], nullptr);
        vkDestroySemaphore(m_device.getDevice(), m_renderFinishedSemaphores[i], nullptr);
        vkDestroyFence(m_device.getDevice(), m_inFlightFences[i], nullptr);
    }
    Core::Logger::info("Renderer", "Sync objects destroyed");

    // Destroy command pool (command buffers are freed automatically)
    if (m_commandPool != VK_NULL_HANDLE) {
        vkDestroyCommandPool(m_device.getDevice(), m_commandPool, nullptr);
    }
    Core::Logger::info("Renderer", "Command pool destroyed");

    Core::Logger::info("Renderer", "Swapchain destroyed");
}

void Swapchain::cleanupSwapchain() {
    // Destroy framebuffers
    for (auto framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_device.getDevice(), framebuffer, nullptr);
    }
    m_framebuffers.clear();

    // Destroy render pass
    if (m_renderPass != VK_NULL_HANDLE) {
        vkDestroyRenderPass(m_device.getDevice(), m_renderPass, nullptr);
        m_renderPass = VK_NULL_HANDLE;
    }

    // Destroy depth buffer resources
    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device.getDevice(), m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }
    if (m_depthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.getAllocator(), m_depthImage, m_depthImageAllocation);
        m_depthImage = VK_NULL_HANDLE;
        m_depthImageAllocation = nullptr;
    }

    // Destroy image views
    for (auto imageView : m_imageViews) {
        vkDestroyImageView(m_device.getDevice(), imageView, nullptr);
    }
    m_imageViews.clear();

    // Destroy swapchain
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device.getDevice(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

void Swapchain::createSwapchain() {
    SwapchainSupportDetails swapchainSupport = querySwapchainSupport(
        m_device.getPhysicalDevice(), m_surface);

    // Validate swapchain support before proceeding
    if (swapchainSupport.formats.empty()) {
        Core::Logger::error("Renderer", "No surface formats available for swapchain");
        throw std::runtime_error("No surface formats available for swapchain");
    }
    if (swapchainSupport.presentModes.empty()) {
        Core::Logger::error("Renderer", "No present modes available for swapchain");
        throw std::runtime_error("No present modes available for swapchain");
    }

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapchainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapchainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapchainSupport.capabilities);

    // Request one more image than minimum for triple buffering if possible
    uint32_t imageCount = swapchainSupport.capabilities.minImageCount + 1;
    if (swapchainSupport.capabilities.maxImageCount > 0 &&
        imageCount > swapchainSupport.capabilities.maxImageCount) {
        imageCount = swapchainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    // Handle queue family sharing mode
    auto indices = m_device.getQueueFamilyIndices();
    uint32_t queueFamilyIndices[] = {
        indices.graphicsFamily.value(),
        indices.presentFamily.value()
    };

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount = 0;
        createInfo.pQueueFamilyIndices = nullptr;
    }

    createInfo.preTransform = swapchainSupport.capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    if (vkCreateSwapchainKHR(m_device.getDevice(), &createInfo, nullptr, &m_swapchain) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create swapchain");
        throw std::runtime_error("Failed to create swapchain");
    }

    // Retrieve swapchain images
    vkGetSwapchainImagesKHR(m_device.getDevice(), m_swapchain, &imageCount, nullptr);
    m_images.resize(imageCount);
    vkGetSwapchainImagesKHR(m_device.getDevice(), m_swapchain, &imageCount, m_images.data());

    m_imageFormat = surfaceFormat.format;
    m_extent = extent;

    // Log present mode name
    const char* presentModeName = "FIFO";
    if (presentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        presentModeName = "MAILBOX";
    } else if (presentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
        presentModeName = "IMMEDIATE";
    }

    Core::Logger::info("Renderer", "Swapchain created (" +
        std::to_string(extent.width) + "x" + std::to_string(extent.height) +
        ", " + std::to_string(imageCount) + " images, " + presentModeName + ")");
}

void Swapchain::createImageViews() {
    m_imageViews.resize(m_images.size());

    for (size_t i = 0; i < m_images.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = m_images[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = m_imageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(m_device.getDevice(), &createInfo, nullptr, &m_imageViews[i]) != VK_SUCCESS) {
            Core::Logger::error("Renderer", "Failed to create image view");
            throw std::runtime_error("Failed to create image view");
        }
    }
    Core::Logger::info("Renderer", "Image views created");
}

VkFormat Swapchain::findDepthFormat() const {
    // Preferred formats in order: D32_SFLOAT (best precision), then D24_UNORM_S8_UINT
    std::vector<VkFormat> candidates = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D32_SFLOAT_S8_UINT,
        VK_FORMAT_D24_UNORM_S8_UINT
    };

    for (VkFormat format : candidates) {
        VkFormatProperties props;
        vkGetPhysicalDeviceFormatProperties(m_device.getPhysicalDevice(), format, &props);

        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) {
            return format;
        }
    }

    Core::Logger::error("Renderer", "Failed to find supported depth format");
    throw std::runtime_error("Failed to find supported depth format");
}

void Swapchain::createDepthResources() {
    m_depthFormat = findDepthFormat();

    // Create depth image via VMA
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = m_extent.width;
    imageInfo.extent.height = m_extent.height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = m_depthFormat;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;

    if (vmaCreateImage(m_device.getAllocator(), &imageInfo, &allocInfo,
                       &m_depthImage, &m_depthImageAllocation, nullptr) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create depth image");
        throw std::runtime_error("Failed to create depth image");
    }

    // Create depth image view
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = m_depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = m_depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(m_device.getDevice(), &viewInfo, nullptr, &m_depthImageView) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create depth image view");
        throw std::runtime_error("Failed to create depth image view");
    }

    // Log depth format with human-readable name
    const char* depthFormatName = "UNKNOWN";
    switch (m_depthFormat) {
        case VK_FORMAT_D32_SFLOAT: depthFormatName = "D32_SFLOAT"; break;
        case VK_FORMAT_D32_SFLOAT_S8_UINT: depthFormatName = "D32_SFLOAT_S8_UINT"; break;
        case VK_FORMAT_D24_UNORM_S8_UINT: depthFormatName = "D24_UNORM_S8_UINT"; break;
        default: break;
    }
    Core::Logger::info("Renderer", "Depth buffer created (format: " + std::string(depthFormatName) + ")");
}

void Swapchain::createRenderPass() {
    // Color attachment
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = m_imageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // Depth attachment
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = m_depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;  // We don't need depth after render
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;  // Index 1 (after color)
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    // Subpass with color and depth
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    // Subpass dependency for proper synchronization (includes depth)
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                              VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    std::array<VkAttachmentDescription, 2> attachments = {colorAttachment, depthAttachment};

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
    renderPassInfo.pAttachments = attachments.data();
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(m_device.getDevice(), &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create render pass");
        throw std::runtime_error("Failed to create render pass");
    }
    Core::Logger::info("Renderer", "Render pass created (with depth attachment)");
}

void Swapchain::createFramebuffers() {
    m_framebuffers.resize(m_imageViews.size());

    for (size_t i = 0; i < m_imageViews.size(); i++) {
        // Attachments: color (per swapchain image) + depth (shared)
        std::array<VkImageView, 2> attachments = {m_imageViews[i], m_depthImageView};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = m_renderPass;
        framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        framebufferInfo.pAttachments = attachments.data();
        framebufferInfo.width = m_extent.width;
        framebufferInfo.height = m_extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(m_device.getDevice(), &framebufferInfo, nullptr, &m_framebuffers[i]) != VK_SUCCESS) {
            Core::Logger::error("Renderer", "Failed to create framebuffer");
            throw std::runtime_error("Failed to create framebuffer");
        }
    }
    Core::Logger::info("Renderer", "Framebuffers created");
}

void Swapchain::createCommandPool() {
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = m_device.getGraphicsQueueFamily();

    if (vkCreateCommandPool(m_device.getDevice(), &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to create command pool");
        throw std::runtime_error("Failed to create command pool");
    }
    Core::Logger::info("Renderer", "Command pool created");
}

void Swapchain::createCommandBuffers() {
    m_commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = m_commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = static_cast<uint32_t>(m_commandBuffers.size());

    if (vkAllocateCommandBuffers(m_device.getDevice(), &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to allocate command buffers");
        throw std::runtime_error("Failed to allocate command buffers");
    }
    Core::Logger::info("Renderer", "Command buffers allocated");
}

void Swapchain::createSyncObjects() {
    m_imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    m_inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;  // Start signaled so first frame doesn't wait

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(m_device.getDevice(), &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device.getDevice(), &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device.getDevice(), &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            Core::Logger::error("Renderer", "Failed to create sync objects");
            throw std::runtime_error("Failed to create sync objects");
        }
    }
    Core::Logger::info("Renderer", "Sync objects created");
}

VkResult Swapchain::acquireNextImage(uint32_t* imageIndex) {
    // Wait for the previous frame with this index to finish
    vkWaitForFences(m_device.getDevice(), 1, &m_inFlightFences[m_currentFrame],
                    VK_TRUE, std::numeric_limits<uint64_t>::max());

    return vkAcquireNextImageKHR(m_device.getDevice(), m_swapchain,
                                  std::numeric_limits<uint64_t>::max(),
                                  m_imageAvailableSemaphores[m_currentFrame],
                                  VK_NULL_HANDLE, imageIndex);
}

VkResult Swapchain::submitAndPresent(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // Reset the fence for this frame
    vkResetFences(m_device.getDevice(), 1, &m_inFlightFences[m_currentFrame]);

    // Submit command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    VkSemaphore signalSemaphores[] = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(m_device.getGraphicsQueue(), 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        Core::Logger::error("Renderer", "Failed to submit draw command buffer");
        throw std::runtime_error("Failed to submit draw command buffer");
    }

    // Present
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapchains[] = {m_swapchain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapchains;
    presentInfo.pImageIndices = &imageIndex;

    VkResult result = vkQueuePresentKHR(m_device.getPresentQueue(), &presentInfo);

    // Advance to next frame
    m_currentFrame = (m_currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

    return result;
}

void Swapchain::recreate() {
    // Handle minimization (window size = 0)
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window.getHandle(), &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(m_window.getHandle(), &width, &height);
        glfwWaitEvents();
    }

    // Wait for device to be idle
    vkDeviceWaitIdle(m_device.getDevice());

    // Store old format to check if render pass needs recreation
    VkFormat oldFormat = m_imageFormat;

    // Cleanup old swapchain (but preserve render pass for now)
    // Destroy framebuffers first (they depend on image views and render pass)
    for (auto framebuffer : m_framebuffers) {
        vkDestroyFramebuffer(m_device.getDevice(), framebuffer, nullptr);
    }
    m_framebuffers.clear();

    // Destroy depth buffer resources (size-dependent)
    if (m_depthImageView != VK_NULL_HANDLE) {
        vkDestroyImageView(m_device.getDevice(), m_depthImageView, nullptr);
        m_depthImageView = VK_NULL_HANDLE;
    }
    if (m_depthImage != VK_NULL_HANDLE) {
        vmaDestroyImage(m_device.getAllocator(), m_depthImage, m_depthImageAllocation);
        m_depthImage = VK_NULL_HANDLE;
        m_depthImageAllocation = nullptr;
    }

    // Destroy image views
    for (auto imageView : m_imageViews) {
        vkDestroyImageView(m_device.getDevice(), imageView, nullptr);
    }
    m_imageViews.clear();

    // Destroy old swapchain
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device.getDevice(), m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }

    // Recreate swapchain, image views, and depth resources
    createSwapchain();
    createImageViews();
    createDepthResources();

    // Only recreate render pass if image format changed (rare)
    if (m_imageFormat != oldFormat) {
        if (m_renderPass != VK_NULL_HANDLE) {
            vkDestroyRenderPass(m_device.getDevice(), m_renderPass, nullptr);
            m_renderPass = VK_NULL_HANDLE;
        }
        createRenderPass();
        Core::Logger::info("Renderer", "Render pass recreated due to format change");
    }

    createFramebuffers();

    Core::Logger::info("Renderer", "Swapchain recreated (" +
        std::to_string(m_extent.width) + "x" + std::to_string(m_extent.height) + ")");
}

SwapchainSupportDetails Swapchain::querySwapchainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapchainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    uint32_t formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}

VkSurfaceFormatKHR Swapchain::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR>& availableFormats) const {
    // Prefer SRGB B8G8R8A8
    for (const auto& format : availableFormats) {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    // Fallback to first available
    return availableFormats[0];
}

VkPresentModeKHR Swapchain::chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR>& availableModes) const {
    // Prefer mailbox (triple buffering, low latency)
    for (const auto& mode : availableModes) {
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    // Fallback to FIFO (always available, vsync)
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Swapchain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities) const {
    // If the surface reports a specific extent, use it
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    }

    // Otherwise, use the window dimensions clamped to the surface capabilities
    int width = 0, height = 0;
    glfwGetFramebufferSize(m_window.getHandle(), &width, &height);

    VkExtent2D extent = {
        static_cast<uint32_t>(width),
        static_cast<uint32_t>(height)
    };

    extent.width = std::clamp(extent.width,
                              capabilities.minImageExtent.width,
                              capabilities.maxImageExtent.width);
    extent.height = std::clamp(extent.height,
                               capabilities.minImageExtent.height,
                               capabilities.maxImageExtent.height);

    return extent;
}

} // namespace Vecna::Renderer
