#pragma once

#include <vulkan/vulkan.h>

#include "Vecna/Scene/ModelInfo.hpp"
#include "Vecna/Loader/GltfPbrLoader.hpp"

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

namespace cgre2 {
class Trackball;
}

namespace Vecna::UI {
class IUIRenderer;
} // namespace Vecna::UI

namespace cgre2 {
class DescriptorPool;
class IndexBuffer;
class Pipeline;
class ShaderManager;
class TextureManager;
class UniformBuffer;
class VertexBuffer;
} // namespace cgre2

namespace Vecna::Vulkan {
class VulkanInstance;
class VulkanDevice;
class Swapchain;
} // namespace Vecna::Vulkan

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

    [[nodiscard]] Vecna::Vulkan::VulkanInstance& getVulkanInstance() { return *m_vulkanInstance; }
    [[nodiscard]] const Vecna::Vulkan::VulkanInstance& getVulkanInstance() const { return *m_vulkanInstance; }

    [[nodiscard]] Vecna::Vulkan::VulkanDevice& getVulkanDevice() { return *m_vulkanDevice; }
    [[nodiscard]] const Vecna::Vulkan::VulkanDevice& getVulkanDevice() const { return *m_vulkanDevice; }

    [[nodiscard]] Vecna::Vulkan::Swapchain& getSwapchain() { return *m_swapchain; }
    [[nodiscard]] const Vecna::Vulkan::Swapchain& getSwapchain() const { return *m_swapchain; }

    [[nodiscard]] cgre2::Pipeline& getPipeline() { return *m_pipeline; }
    [[nodiscard]] const cgre2::Pipeline& getPipeline() const { return *m_pipeline; }

private:
    void createSurface();
    void createPipeline();
    void createPbrPipeline();
    void createCube();
    void drawFrame();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void updateCameraUbo(float aspect);

    void handleKeyboardShortcuts();
    void onShadingModeChanged(bool flat);
    void onShowNormalsChanged(bool show);
    void onToonChanged(bool toon);
    void onOutlineChanged(bool outline);
    void onPbrChanged(bool pbr);

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
    std::unique_ptr<Vecna::Vulkan::VulkanInstance> m_vulkanInstance;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    std::unique_ptr<Vecna::Vulkan::VulkanDevice> m_vulkanDevice;
    std::unique_ptr<Vecna::Vulkan::Swapchain> m_swapchain;
    // Caches VkShaderModule + SPIR-V reflection metadata. Lives between
    // pipeline recreations and is destroyed AFTER the last Pipeline so
    // every VkShaderModule outlives the pipelines that reference it.
    std::unique_ptr<cgre2::ShaderManager> m_shaderManager;
    std::unique_ptr<cgre2::Pipeline> m_pipeline;
    // Inverted-hull outline pass — same vertex/index buffers as the main
    // mesh, different shaders + cullMode=FRONT. Lifetime mirrors m_pipeline.
    std::unique_ptr<cgre2::Pipeline> m_outlinePipeline;

    // Geometry buffers (cube default or loaded model)
    // IMPORTANT: Must be destroyed BEFORE VulkanDevice (requires VMA for cleanup)
    std::unique_ptr<cgre2::VertexBuffer> m_vertexBuffer;
    std::unique_ptr<cgre2::IndexBuffer> m_indexBuffer;

    // Trackball rotation (Story 4-2)
    std::unique_ptr<cgre2::Trackball> m_trackball;

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

    // Flat shading mode (specialization constant in pipeline)
    bool m_flatShading = false;
    bool m_pendingShadingChange = false;

    // Debug overlay: switches the fragment shader from basic.frag (Lambert
    // diffuse) to normals.frag (normal-as-RGB). Smoke test for the cgre2
    // shader-pipeline plumbing — a second .spv goes through ShaderManager,
    // a second Pipeline is built sharing the same vertex module from the
    // cache, and the basic.frag-only spec constant `useFlat` is silently
    // ignored by normals.frag.
    bool m_showNormals = false;
    bool m_pendingShowNormalsChange = false;

    // Toon (cel) shading: posterizes the diffuse term into a few discrete
    // bands. Precedence order in createPipeline:
    //   m_showNormals > m_useToon > m_flatShading (via basic.frag)
    bool m_useToon = false;
    bool m_pendingToonChange = false;

    // Inverted-hull outline: an extra pass with a dedicated pipeline that
    // culls front faces and inflates each vertex along its normal, drawn
    // BEFORE the main mesh so depth testing leaves only the silhouette
    // visible. Independent of the main shader choice.
    bool m_useOutline = false;
    bool m_pendingOutlineChange = false;

    // Deferred model load (avoids buffer destruction mid-render when triggered from ImGui menu)
    std::optional<std::filesystem::path> m_pendingModelPath;

    // Model metadata (Story 5-3)
    Scene::ModelInfo m_modelInfo;

    // UI renderer abstraction (Story 5-1)
    // IMPORTANT: Must be destroyed BEFORE VulkanDevice (uses Vulkan resources)
    std::unique_ptr<UI::IUIRenderer> m_uiRenderer;

    // PBR rendering (Cook-Torrance + glTF) — additions from the PBR story.
    // Lifetime: all VkDescriptorSetLayout, VkDescriptorSet, and unique_ptr-
    // wrapped cgre2 resources below must be released BEFORE m_vulkanDevice
    // (they hold VkDescriptorSetLayout handles and VMA-allocated buffers).
    std::unique_ptr<cgre2::TextureManager> m_textureManager;
    std::unique_ptr<cgre2::DescriptorPool> m_descriptorPool;
    std::unique_ptr<cgre2::Pipeline>       m_pbrPipeline;

    VkDescriptorSetLayout m_sceneSetLayout     = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_materialSetLayout  = VK_NULL_HANDLE;
    VkDescriptorSet       m_sceneSet           = VK_NULL_HANDLE;
    VkDescriptorSet       m_defaultMaterialSet = VK_NULL_HANDLE;

    std::unique_ptr<cgre2::UniformBuffer> m_cameraUbo;
    std::unique_ptr<cgre2::UniformBuffer> m_lightsUbo;
    std::unique_ptr<cgre2::UniformBuffer> m_defaultMaterialUbo;

    // VertexPBR-typed buffer used by the PBR pipeline. Built alongside
    // m_vertexBuffer (basic Vertex layout) at model-load time so the
    // renderer can switch between basic and PBR pipelines without
    // re-uploading geometry.
    std::unique_ptr<cgre2::VertexBuffer> m_pbrVertexBuffer;

    // PBR scene loaded from glTF — per-primitive vertex/index buffers
    // + materials. Reset via `m_pbrScene = {}` before reloading.
    Loader::PBRScene m_pbrScene;
    float            m_pbrSceneCenter[3] = {0.0f, 0.0f, 0.0f};

    // Cook-Torrance PBR toggle (UI checkbox).
    bool m_usePbr           = false;
    bool m_pendingPbrChange = false;
};

} // namespace Vecna::Core
