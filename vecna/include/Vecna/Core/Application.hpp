#pragma once

#include <vulkan/vulkan.h>

#include <filesystem>
#include <memory>

namespace Vecna::Core {
class Window;
} // namespace Vecna::Core

namespace Vecna::Renderer {
class IndexBuffer;
class Pipeline;
class Swapchain;
class VertexBuffer;
class VulkanDevice;
class VulkanInstance;
} // namespace Vecna::Renderer

namespace Vecna::Core {

class Application {
public:
    Application();
    ~Application();

    // Non-copyable, non-movable
    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;
    Application(Application&&) = delete;
    Application& operator=(Application&&) = delete;

    void run();

    [[nodiscard]] Window& getWindow() { return *m_window; }
    [[nodiscard]] const Window& getWindow() const { return *m_window; }

    [[nodiscard]] Renderer::VulkanInstance& getVulkanInstance() { return *m_vulkanInstance; }
    [[nodiscard]] const Renderer::VulkanInstance& getVulkanInstance() const { return *m_vulkanInstance; }

    [[nodiscard]] Renderer::VulkanDevice& getVulkanDevice() { return *m_vulkanDevice; }
    [[nodiscard]] const Renderer::VulkanDevice& getVulkanDevice() const { return *m_vulkanDevice; }

    [[nodiscard]] Renderer::Swapchain& getSwapchain() { return *m_swapchain; }
    [[nodiscard]] const Renderer::Swapchain& getSwapchain() const { return *m_swapchain; }

    [[nodiscard]] Renderer::Pipeline& getPipeline() { return *m_pipeline; }
    [[nodiscard]] const Renderer::Pipeline& getPipeline() const { return *m_pipeline; }

private:
    void createSurface();
    void createPipeline();
    void createCube();
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);

    // ImGUI integration (Story 3-4)
    void initImGui();
    void shutdownImGui();
    void renderImGui(VkCommandBuffer commandBuffer);
    void renderMenuBar();
    void handleKeyboardShortcuts();

    // File operations (Story 3-4)
    void openFileDialog();
    void loadModel(const std::filesystem::path& path);

    // Creation order: Window → VulkanInstance → Surface → VulkanDevice → Swapchain → Pipeline
    // Destruction order (explicit in destructor): Pipeline → Swapchain → VulkanDevice → Surface → VulkanInstance → Window
    // This ensures Vulkan resources are released before GLFW terminates.
    // IMPORTANT: Pipeline must be destroyed BEFORE Swapchain (references render pass).
    std::unique_ptr<Window> m_window;
    std::unique_ptr<Renderer::VulkanInstance> m_vulkanInstance;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    std::unique_ptr<Renderer::VulkanDevice> m_vulkanDevice;
    std::unique_ptr<Renderer::Swapchain> m_swapchain;
    std::unique_ptr<Renderer::Pipeline> m_pipeline;

    // Cube test geometry (Story 2-4)
    // IMPORTANT: Must be destroyed BEFORE VulkanDevice (requires VMA for cleanup)
    std::unique_ptr<Renderer::VertexBuffer> m_cubeVertexBuffer;
    std::unique_ptr<Renderer::IndexBuffer> m_cubeIndexBuffer;

    // Time tracking for animation
    float m_rotationAngle = 0.0f;

    // ImGUI resources (Story 3-4)
    VkDescriptorPool m_imguiDescriptorPool = VK_NULL_HANDLE;
    bool m_imguiInitialized = false;
};

} // namespace Vecna::Core
