#include "Vecna/UI/ImGuiRenderer.hpp"
#include "Vecna/Core/Logger.hpp"
#include "Vecna/Core/Window.hpp"
#include "Vecna/Renderer/Swapchain.hpp"
#include "Vecna/Renderer/VulkanDevice.hpp"
#include "Vecna/Renderer/VulkanInstance.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>
#include <GLFW/glfw3.h>

#include <array>
#include <stdexcept>

namespace Vecna::UI {

ImGuiRenderer::ImGuiRenderer(Renderer::VulkanInstance& vulkanInstance,
                             Renderer::VulkanDevice& vulkanDevice,
                             Renderer::Swapchain& swapchain,
                             Core::Window& window)
    : m_vulkanInstance(vulkanInstance)
    , m_vulkanDevice(vulkanDevice)
    , m_swapchain(swapchain)
    , m_window(window) {
}

ImGuiRenderer::~ImGuiRenderer() {
    shutdown();
}

void ImGuiRenderer::init() {
    if (m_initialized) {
        return;
    }

    // Create descriptor pool for ImGui
    // Conservative sizes sufficient for menu bar and basic UI elements
    VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 10},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10},
        {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10},
        {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 10},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 10},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 10},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 10}
    };

    VkDescriptorPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets = 100;
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(m_vulkanDevice.getDevice(), &poolInfo, nullptr,
                               &m_descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGui descriptor pool");
    }

    // Initialize ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style
    ImGui::StyleColorsDark();

    // Initialize ImGui GLFW backend
    // install_callbacks=true chains to previously-installed GLFW callbacks
    ImGui_ImplGlfw_InitForVulkan(m_window.getHandle(), true);

    // Initialize ImGui Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_vulkanInstance.getInstance();
    initInfo.PhysicalDevice = m_vulkanDevice.getPhysicalDevice();
    initInfo.Device = m_vulkanDevice.getDevice();
    initInfo.QueueFamily = m_vulkanDevice.getGraphicsQueueFamily();
    initInfo.Queue = m_vulkanDevice.getGraphicsQueue();
    initInfo.DescriptorPool = m_descriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<uint32_t>(m_swapchain.getImageCount());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Subpass = 0;

    ImGui_ImplVulkan_Init(&initInfo, m_swapchain.getRenderPass());

    // Upload fonts
    ImGui_ImplVulkan_CreateFontsTexture();

    m_initialized = true;
    Core::Logger::info("UI", "ImGui initialized");
}

void ImGuiRenderer::shutdown() {
    if (!m_initialized) {
        return;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_descriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_vulkanDevice.getDevice(), m_descriptorPool, nullptr);
        m_descriptorPool = VK_NULL_HANDLE;
    }

    m_initialized = false;
    Core::Logger::info("UI", "ImGui shutdown");
}

void ImGuiRenderer::beginFrame() {
    if (!m_initialized) {
        return;
    }

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render UI widgets
    renderMenuBar();
    renderInfoPanel();
    renderErrorMessages();
}

void ImGuiRenderer::render(VkCommandBuffer commandBuffer) {
    if (!m_initialized) {
        return;
    }

    // Finalize ImGui frame and record draw commands
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void ImGuiRenderer::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Fichier")) {
            if (ImGui::MenuItem("Ouvrir...", "Ctrl+O")) {
                if (m_onFileOpen) {
                    m_onFileOpen();
                }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quitter", "Alt+F4")) {
                m_window.close();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void ImGuiRenderer::setModelInfo(const Scene::ModelInfo& info) {
    m_modelInfo = info;
    m_hasModel = true;
}

void ImGuiRenderer::renderInfoPanel() {
    if (!m_hasModel) {
        return;
    }

    // Position: top-right corner, below menu bar
    constexpr float padding = 10.0f;
    float menuBarHeight = ImGui::GetFrameHeight();
    ImVec2 windowPos(ImGui::GetIO().DisplaySize.x - padding, menuBarHeight + padding);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(1.0f, 0.0f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("Infos Modele", nullptr, flags)) {
        // Filename
        ImGui::Text("Fichier: %s", m_modelInfo.filename.c_str());
        ImGui::Separator();

        // Geometry stats
        ImGui::Text("Sommets: %u", m_modelInfo.vertexCount);
        ImGui::Text("Triangles: %u", m_modelInfo.triangleCount);
        ImGui::Separator();

        // Bounding box dimensions (pre-computed in ModelInfo::computeDerived)
        ImGui::Text("Dimensions: %.3f x %.3f x %.3f",
                     m_modelInfo.dimensions[0], m_modelInfo.dimensions[1], m_modelInfo.dimensions[2]);

        // Center (pre-computed)
        ImGui::Text("Centre: (%.3f, %.3f, %.3f)",
                     m_modelInfo.center[0], m_modelInfo.center[1], m_modelInfo.center[2]);

        // Min / Max
        ImGui::Text("Min: (%.3f, %.3f, %.3f)",
                     m_modelInfo.bboxMin[0], m_modelInfo.bboxMin[1], m_modelInfo.bboxMin[2]);
        ImGui::Text("Max: (%.3f, %.3f, %.3f)",
                     m_modelInfo.bboxMax[0], m_modelInfo.bboxMax[1], m_modelInfo.bboxMax[2]);
    }
    ImGui::End();
}

void ImGuiRenderer::showErrorMessage(const std::string& message) {
    m_errorMessages.push_back({message, std::chrono::steady_clock::now()});
}

void ImGuiRenderer::renderErrorMessages() {
    if (m_errorMessages.empty()) {
        return;
    }

    // Remove expired messages (AC2: auto-dismiss after 5s)
    auto now = std::chrono::steady_clock::now();
    std::erase_if(m_errorMessages, [&](const ErrorMessage& msg) {
        auto elapsed = std::chrono::duration<float>(now - msg.timestamp).count();
        return elapsed > ERROR_DISPLAY_DURATION;
    });

    if (m_errorMessages.empty()) {
        return;
    }

    // Position: bottom-left corner
    constexpr float padding = 10.0f;
    ImVec2 windowPos(padding, ImGui::GetIO().DisplaySize.y - padding);
    ImGui::SetNextWindowPos(windowPos, ImGuiCond_Always, ImVec2(0.0f, 1.0f));

    // Semi-transparent dark red background
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.4f, 0.05f, 0.05f, 0.85f));

    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize |
                             ImGuiWindowFlags_NoMove |
                             ImGuiWindowFlags_NoSavedSettings |
                             ImGuiWindowFlags_NoTitleBar |
                             ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav;

    if (ImGui::Begin("##ErrorMessages", nullptr, flags)) {
        int indexToRemove = -1;
        for (int i = 0; i < static_cast<int>(m_errorMessages.size()); ++i) {
            if (i > 0) {
                ImGui::Separator();
            }
            ImGui::TextUnformatted(m_errorMessages[i].text.c_str());
            ImGui::SameLine();
            ImGui::PushID(i);
            if (ImGui::SmallButton("X")) {
                indexToRemove = i;
            }
            ImGui::PopID();
        }
        // Remove manually dismissed message (AC3)
        if (indexToRemove >= 0) {
            m_errorMessages.erase(m_errorMessages.begin() + indexToRemove);
        }
    }
    ImGui::End();

    ImGui::PopStyleColor();
}

} // namespace Vecna::UI
