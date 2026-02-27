#pragma once

#include <vulkan/vulkan.h>

#include "Vecna/Scene/ModelInfo.hpp"

#include <filesystem>
#include <memory>
#include <optional>

struct GLFWwindow;

// Forward-declare cgmath camera (full include in .cpp behind #pragma warning suppression)
#include "src/cgmath/StorageOrder.h"
template <class TValue, StorageOrder Order> class TCamera;
using Cameraf = TCamera<float, StorageOrder::ColumnMajor>;

namespace Vecna::Core {
class Window;
} // namespace Vecna::Core

namespace Vecna::Scene {
class Trackball;
} // namespace Vecna::Scene

namespace Vecna::UI {
class IUIRenderer;
} // namespace Vecna::UI

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

    void handleKeyboardShortcuts();

    // File operations (Story 3-4)
    void openFileDialog();
    void loadModel(const std::filesystem::path& path);

    // UI error notification helper (Story 5-5)
    void showUIError(const std::string& message);

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

    // Geometry buffers (cube default or loaded model)
    // IMPORTANT: Must be destroyed BEFORE VulkanDevice (requires VMA for cleanup)
    std::unique_ptr<Renderer::VertexBuffer> m_vertexBuffer;
    std::unique_ptr<Renderer::IndexBuffer> m_indexBuffer;

    // Trackball rotation (Story 4-2)
    std::unique_ptr<Scene::Trackball> m_trackball;

    // GLFW callbacks (routed via glfwSetWindowUserPointer → Application*)
    static void framebufferResizeCallback(GLFWwindow* window, int width, int height);
    static void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
    static void cursorPosCallback(GLFWwindow* window, double xpos, double ypos);
    static void scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
    static void dropCallback(GLFWwindow* window, int count, const char** paths);

    // Camera (position, target, projection — adapted to loaded model size)
    // std::unique_ptr to keep TCamera.h include confined to the .cpp
    // (TVector3.h has 'using namespace std' that would pollute all consumers)
    std::unique_ptr<Cameraf> m_camera;
    float m_cameraDistance = 3.0f;  // Authoritative camera distance (matches TCamera default Z=3)

    // Pan state (Story 4-4)
    bool m_panning = false;
    double m_lastPanX = 0.0;
    double m_lastPanY = 0.0;

    // Deferred model load (avoids buffer destruction mid-render when triggered from ImGui menu)
    std::optional<std::filesystem::path> m_pendingModelPath;

    // Model metadata (Story 5-3)
    Scene::ModelInfo m_modelInfo;

    // UI renderer abstraction (Story 5-1)
    // IMPORTANT: Must be destroyed BEFORE VulkanDevice (uses Vulkan resources)
    std::unique_ptr<UI::IUIRenderer> m_uiRenderer;
};

} // namespace Vecna::Core
