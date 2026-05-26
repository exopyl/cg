#pragma once

#include "Vecna/Scene/ModelInfo.hpp"

#include <vulkan/vulkan.h>

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace Vecna::Vulkan {
class VulkanInstance;
class VulkanDevice;
class Swapchain;
} // namespace Vecna::Vulkan

namespace Vecna::Core { class Window; }

namespace Vecna::UI {

/// Abstract UI renderer interface — decouples Application from any
/// specific UI library (ImGui, Qt, …) so backends can be swapped without
/// touching the application logic.
class IUIRenderer {
public:
    virtual ~IUIRenderer() = default;

    virtual void init() = 0;
    virtual void beginFrame() = 0;
    virtual void render(VkCommandBuffer commandBuffer) = 0;

    /// Called by Application when the swapchain is recreated (e.g. window
    /// resize). Implementations that hold render-pass-dependent resources
    /// rebuild them here.
    virtual void onSwapchainRecreated() = 0;
};

/// ImGui implementation of IUIRenderer. Owns a private VkDescriptorPool
/// dedicated to ImGui's textures, plus the menu-bar + info-panel + error-
/// toast UI logic.
///
/// Lifetime: must be destroyed BEFORE VulkanDevice (uses Vulkan resources).
class ImGuiRenderer : public IUIRenderer {
public:
    ImGuiRenderer(Vecna::Vulkan::VulkanInstance& vulkanInstance,
                  Vecna::Vulkan::VulkanDevice&   vulkanDevice,
                  Vecna::Vulkan::Swapchain&      swapchain,
                  Core::Window&          window);
    ~ImGuiRenderer() override;

    ImGuiRenderer(const ImGuiRenderer&) = delete;
    ImGuiRenderer& operator=(const ImGuiRenderer&) = delete;
    ImGuiRenderer(ImGuiRenderer&&) = delete;
    ImGuiRenderer& operator=(ImGuiRenderer&&) = delete;

    void init() override;
    void beginFrame() override;
    void render(VkCommandBuffer commandBuffer) override;
    void onSwapchainRecreated() override {}

    // Menu-bar callback setters — Application wires these to its own
    // handlers in createUI(). Each is invoked once per user interaction.
    void setOnFileOpen           (std::function<void()>     cb) { m_onFileOpen            = std::move(cb); }
    void setOnShadingModeChanged (std::function<void(bool)> cb) { m_onShadingModeChanged  = std::move(cb); }
    void setOnShowNormalsChanged (std::function<void(bool)> cb) { m_onShowNormalsChanged  = std::move(cb); }
    void setOnToonChanged        (std::function<void(bool)> cb) { m_onToonChanged         = std::move(cb); }
    void setOnOutlineChanged     (std::function<void(bool)> cb) { m_onOutlineChanged      = std::move(cb); }
    void setOnPbrChanged         (std::function<void(bool)> cb) { m_onPbrChanged          = std::move(cb); }

    /// Refresh the info-panel content. Called when a model loads.
    void setModelInfo(const Scene::ModelInfo& info);

    /// Append an error toast in the bottom-left corner. Auto-dismissed
    /// after ERROR_DISPLAY_DURATION seconds (or user-dismissed via "X").
    void showErrorMessage(const std::string& message);

private:
    void shutdown();
    void renderMenuBar();
    void renderInfoPanel();
    void renderErrorMessages();

    Vecna::Vulkan::VulkanInstance& m_vulkanInstance;
    Vecna::Vulkan::VulkanDevice&   m_vulkanDevice;
    Vecna::Vulkan::Swapchain&      m_swapchain;
    Core::Window&          m_window;

    bool             m_initialized    = false;
    VkDescriptorPool m_descriptorPool = VK_NULL_HANDLE;

    std::function<void()>     m_onFileOpen;
    std::function<void(bool)> m_onShadingModeChanged;
    std::function<void(bool)> m_onShowNormalsChanged;
    std::function<void(bool)> m_onToonChanged;
    std::function<void(bool)> m_onOutlineChanged;
    std::function<void(bool)> m_onPbrChanged;

    // Menu-bar checkbox state — kept here so ImGui::Checkbox sees a stable
    // bool address across frames.
    bool m_flatShading = false;
    bool m_showNormals = false;
    bool m_useToon     = false;
    bool m_useOutline  = false;
    bool m_usePbr      = false;

    Scene::ModelInfo m_modelInfo;
    bool             m_hasModel = false;

    struct ErrorMessage {
        std::string                           text;
        std::chrono::steady_clock::time_point timestamp;
    };
    std::vector<ErrorMessage> m_errorMessages;

    static constexpr float ERROR_DISPLAY_DURATION = 5.0f; // seconds
};

} // namespace Vecna::UI
