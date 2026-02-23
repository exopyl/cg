#include "Vecna/Core/Application.hpp"
#include "Vecna/Core/FileDialog.hpp"
#include "Vecna/Core/Window.hpp"
#include "Vecna/Core/Logger.hpp"
#include "Vecna/Renderer/Buffer.hpp"
#include "Vecna/Renderer/Pipeline.hpp"
#include "Vecna/Renderer/Swapchain.hpp"
#include "Vecna/Renderer/VulkanDevice.hpp"
#include "Vecna/Renderer/VulkanInstance.hpp"
#include "TMatrix4.h"

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>

// Shader directory search paths (in order of priority)
// Supports running from: build/Debug, build/Release, build/, project root, or installed location
static const std::array<std::string, 5> SHADER_SEARCH_PATHS = {
    "shaders/compiled",           // Running from project root
    "../shaders/compiled",        // Running from build/
    "../../shaders/compiled",     // Running from build/Debug or build/Release
    "../../../shaders/compiled",  // Running from build/Debug/tests or similar
    "./shaders"                   // Installed location (shaders next to executable)
};

/// Find the shader directory by checking multiple possible locations.
/// @return Path to shader directory, or empty string if not found.
static std::string findShaderDirectory() {
    for (const auto& path : SHADER_SEARCH_PATHS) {
        // Check if basic.vert.spv exists in this path
        std::string testFile = path + "/basic.vert.spv";
        std::ifstream file(testFile, std::ios::binary);
        if (file.good()) {
            return path;
        }
    }
    return "";  // Not found
}

namespace Vecna::Core {

// Clear color for the render pass (dark blue)
static constexpr VkClearColorValue CLEAR_COLOR = {{0.1f, 0.1f, 0.2f, 1.0f}};

// Camera and projection constants
static constexpr float ROTATION_SPEED = 0.785398f;  // π/4 radians per second (~45°/s)
static constexpr float CAMERA_DISTANCE = 3.0f;      // Distance from origin on Z axis
static constexpr float FOV_RADIANS = 0.785398f;     // 45 degrees field of view
static constexpr float NEAR_PLANE = 0.1f;           // Near clipping plane
static constexpr float FAR_PLANE = 100.0f;          // Far clipping plane

Application::Application() {
    Logger::info("Core", "Application starting");

    // Create window with default configuration (1280x720, "Vecna", resizable)
    // Window must be created BEFORE VulkanInstance (GLFW must be initialized for extensions)
    m_window = std::make_unique<Window>();

    // Create Vulkan instance (requires GLFW to be initialized)
    m_vulkanInstance = std::make_unique<Renderer::VulkanInstance>();

    // Create surface (requires Window and VulkanInstance)
    createSurface();

    // Create Vulkan device (selects GPU and creates logical device)
    // Requires surface for present queue family detection
    m_vulkanDevice = std::make_unique<Renderer::VulkanDevice>(*m_vulkanInstance, m_surface);

    // Create swapchain (requires device and surface)
    m_swapchain = std::make_unique<Renderer::Swapchain>(*m_vulkanDevice, m_surface, *m_window);

    // Create graphics pipeline (requires device and render pass from swapchain)
    createPipeline();

    // Create cube test geometry (Story 2-4)
    createCube();

    // Initialize ImGUI (Story 3-4)
    initImGui();

    Logger::info("Core", "Application initialized");
}

Application::~Application() {
    Logger::info("Core", "Application shutting down");

    // Wait for GPU to finish before cleanup
    if (m_vulkanDevice) {
        vkDeviceWaitIdle(m_vulkanDevice->getDevice());
    }

    // Shutdown ImGUI before destroying Vulkan resources (Story 3-4)
    shutdownImGui();

    // Destroy in reverse order of creation
    // Cube buffers → Pipeline → Swapchain → VulkanDevice → Surface → VulkanInstance → Window (GLFW)
    // IMPORTANT: Cube buffers must be destroyed BEFORE VulkanDevice (require VMA allocator)
    // IMPORTANT: Pipeline must be destroyed BEFORE Swapchain (references render pass)
    m_cubeIndexBuffer.reset();
    m_cubeVertexBuffer.reset();
    m_pipeline.reset();
    m_swapchain.reset();
    m_vulkanDevice.reset();

    if (m_surface != VK_NULL_HANDLE) {
        vkDestroySurfaceKHR(m_vulkanInstance->getInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
        Logger::info("Core", "Surface destroyed");
    }

    m_vulkanInstance.reset();
    m_window.reset();

    Logger::info("Core", "Application terminated");
}

void Application::createSurface() {
    if (glfwCreateWindowSurface(m_vulkanInstance->getInstance(), m_window->getHandle(),
                                nullptr, &m_surface) != VK_SUCCESS) {
        Logger::error("Core", "Failed to create window surface");
        throw std::runtime_error("Failed to create window surface");
    }
    Logger::info("Core", "Surface created");
}

void Application::createPipeline() {
    std::string shaderDir = findShaderDirectory();
    if (shaderDir.empty()) {
        Logger::error("Core", "Failed to find shader directory. Searched paths:");
        for (const auto& path : SHADER_SEARCH_PATHS) {
            Logger::error("Core", "  - " + path);
        }
        throw std::runtime_error("Failed to find shader directory");
    }

    m_pipeline = std::make_unique<Renderer::Pipeline>(
        *m_vulkanDevice,
        m_swapchain->getRenderPass(),
        shaderDir
    );
}

void Application::createCube() {
    // Cube geometry with flat shading (24 vertices, 4 per face)
    // Each face has distinct normals and colors for visibility
    // CCW winding order for front-facing triangles
    static const std::vector<Renderer::Vertex> vertices = {
        // Front face (Z+) - Red
        {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}, {1.0f, 0.0f, 0.0f}},

        // Back face (Z-) - Green
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}, {0.0f, 1.0f, 0.0f}},

        // Top face (Y+) - Blue
        {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}, {0.0f, 0.0f, 1.0f}},

        // Bottom face (Y-) - Yellow
        {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f, 0.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f, 0.0f}},
        {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f, 0.0f}},
        {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}, {1.0f, 1.0f, 0.0f}},

        // Right face (X+) - Magenta
        {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f, 1.0f}},
        {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f, 1.0f}},
        {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}, {1.0f, 0.0f, 1.0f}},

        // Left face (X-) - Cyan
        {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f, 1.0f}},
        {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f, 1.0f}},
        {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f, 1.0f}},
        {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}, {0.0f, 1.0f, 1.0f}},
    };

    // 6 faces × 2 triangles × 3 indices = 36 indices
    static const std::vector<uint32_t> indices = {
        0, 1, 2,  2, 3, 0,       // Front
        4, 5, 6,  6, 7, 4,       // Back
        8, 9, 10,  10, 11, 8,    // Top
        12, 13, 14,  14, 15, 12, // Bottom
        16, 17, 18,  18, 19, 16, // Right
        20, 21, 22,  22, 23, 20, // Left
    };

    m_cubeVertexBuffer = std::make_unique<Renderer::VertexBuffer>(*m_vulkanDevice, vertices);
    m_cubeIndexBuffer = std::make_unique<Renderer::IndexBuffer>(*m_vulkanDevice, indices);

    Logger::info("Core", "Cube buffers created (vertex: " +
                 std::to_string(m_cubeVertexBuffer->getSize()) + " bytes, index: " +
                 std::to_string(m_cubeIndexBuffer->getSize()) + " bytes)");
}

void Application::run() {
    Logger::info("Core", "Entering main loop");

    auto lastTime = glfwGetTime();

    while (!m_window->shouldClose()) {
        m_window->pollEvents();

        // Handle keyboard shortcuts (Story 3-4)
        handleKeyboardShortcuts();

        // Update rotation angle based on elapsed time
        auto currentTime = glfwGetTime();
        auto deltaTime = static_cast<float>(currentTime - lastTime);
        lastTime = currentTime;

        // Rotate based on elapsed time
        m_rotationAngle += deltaTime * ROTATION_SPEED;

        drawFrame();
    }

    // Wait for the device to finish before exiting
    vkDeviceWaitIdle(m_vulkanDevice->getDevice());

    Logger::info("Core", "Exiting main loop");
}

void Application::drawFrame() {
    // Acquire next image
    uint32_t imageIndex = 0;
    VkResult result = m_swapchain->acquireNextImage(&imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        // Recreate swapchain and pipeline (render pass may have changed)
        m_pipeline.reset();
        m_swapchain->recreate();
        createPipeline();
        return;
    }
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("Failed to acquire swapchain image");
    }

    // Get the command buffer for this frame
    VkCommandBuffer commandBuffer = m_swapchain->getCommandBuffer();

    // Reset and record command buffer
    vkResetCommandBuffer(commandBuffer, 0);
    recordCommandBuffer(commandBuffer, imageIndex);

    // Submit and present
    result = m_swapchain->submitAndPresent(commandBuffer, imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_window->wasResized()) {
        m_window->resetResizedFlag();
        // Recreate swapchain and pipeline (render pass may have changed)
        m_pipeline.reset();
        m_swapchain->recreate();
        createPipeline();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("Failed to present swapchain image");
    }
}

void Application::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = nullptr;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("Failed to begin recording command buffer");
    }

    // Begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = m_swapchain->getRenderPass();
    renderPassInfo.framebuffer = m_swapchain->getFramebuffer(imageIndex);
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = m_swapchain->getExtent();

    // Two clear values: color and depth
    std::array<VkClearValue, 2> clearValues{};
    clearValues[0].color = CLEAR_COLOR;
    clearValues[1].depthStencil = {1.0f, 0};  // Depth = 1.0 (far plane)

    renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
    renderPassInfo.pClearValues = clearValues.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Bind the graphics pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline->getPipeline());

    // Set dynamic viewport
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(m_swapchain->getExtent().width);
    viewport.height = static_cast<float>(m_swapchain->getExtent().height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    // Set dynamic scissor
    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = m_swapchain->getExtent();
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Validate buffers exist before binding (defensive check)
    if (!m_cubeVertexBuffer || !m_cubeIndexBuffer) {
        throw std::runtime_error("Cube buffers not initialized");
    }

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {m_cubeVertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, m_cubeIndexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

    // Calculate MVP matrix using TMatrix4 (column-major, Vulkan-ready)
    Matrix4f model;
    model.SetRotateY(-m_rotationAngle);  // Negated: TMatrix4 uses right-hand convention

    float eye[3] = {0.0f, 0.0f, CAMERA_DISTANCE};
    float center[3] = {0.0f, 0.0f, 0.0f};
    float up[3] = {0.0f, 1.0f, 0.0f};
    Matrix4f view;
    view.SetLookAt(eye, center, up);

    float aspect = viewport.width / viewport.height;
    Matrix4f projection;
    projection.SetPerspective(FOV_RADIANS, aspect, NEAR_PLANE, FAR_PLANE);

    // MVP = Projection * View * Model
    Matrix4f mvp = projection * view * model;

    // Push MVP matrix (ColumnMajor data() is directly Vulkan-compatible)
    Renderer::PushConstants pushConstants{};
    std::copy(mvp.data(), mvp.data() + 16, pushConstants.mvp);
    vkCmdPushConstants(commandBuffer, m_pipeline->getPipelineLayout(),
                       VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(Renderer::PushConstants), &pushConstants);

    // Draw indexed cube
    vkCmdDrawIndexed(commandBuffer, m_cubeIndexBuffer->getIndexCount(), 1, 0, 0, 0);

    // Render ImGUI overlay (Story 3-4)
    renderImGui(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
    }
}

// ============================================================================
// ImGUI Integration (Story 3-4)
// ============================================================================

void Application::initImGui() {
    // Create descriptor pool for ImGUI
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
    poolInfo.maxSets = 100;  // Sufficient for ImGUI needs
    poolInfo.poolSizeCount = static_cast<uint32_t>(std::size(poolSizes));
    poolInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(m_vulkanDevice->getDevice(), &poolInfo, nullptr,
                               &m_imguiDescriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create ImGUI descriptor pool");
    }

    // Initialize ImGUI context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup style
    ImGui::StyleColorsDark();

    // Initialize ImGUI GLFW backend
    ImGui_ImplGlfw_InitForVulkan(m_window->getHandle(), true);

    // Initialize ImGUI Vulkan backend
    ImGui_ImplVulkan_InitInfo initInfo{};
    initInfo.Instance = m_vulkanInstance->getInstance();
    initInfo.PhysicalDevice = m_vulkanDevice->getPhysicalDevice();
    initInfo.Device = m_vulkanDevice->getDevice();
    initInfo.QueueFamily = m_vulkanDevice->getGraphicsQueueFamily();
    initInfo.Queue = m_vulkanDevice->getGraphicsQueue();
    initInfo.DescriptorPool = m_imguiDescriptorPool;
    initInfo.MinImageCount = 2;
    initInfo.ImageCount = static_cast<uint32_t>(m_swapchain->getImageCount());
    initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    initInfo.Subpass = 0;

    ImGui_ImplVulkan_Init(&initInfo, m_swapchain->getRenderPass());

    // Upload fonts
    ImGui_ImplVulkan_CreateFontsTexture();

    m_imguiInitialized = true;
    Logger::info("UI", "ImGUI initialized");
}

void Application::shutdownImGui() {
    if (!m_imguiInitialized) {
        return;
    }

    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    if (m_imguiDescriptorPool != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(m_vulkanDevice->getDevice(), m_imguiDescriptorPool, nullptr);
        m_imguiDescriptorPool = VK_NULL_HANDLE;
    }

    m_imguiInitialized = false;
    Logger::info("UI", "ImGUI shutdown");
}

void Application::renderImGui(VkCommandBuffer commandBuffer) {
    if (!m_imguiInitialized) {
        return;
    }

    // Start new ImGUI frame
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Render menu bar
    renderMenuBar();

    // Finalize ImGUI frame
    ImGui::Render();

    // Record ImGUI draw commands
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
}

void Application::renderMenuBar() {
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Fichier")) {
            if (ImGui::MenuItem("Ouvrir...", "Ctrl+O")) {
                openFileDialog();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Quitter", "Alt+F4")) {
                m_window->close();
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
}

void Application::handleKeyboardShortcuts() {
    // Check for Ctrl+O (open file)
    // Track key state outside the Ctrl check to properly handle release
    static bool oKeyWasPressed = false;
    bool ctrlPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                       glfwGetKey(m_window->getHandle(), GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS;
    bool oKeyPressed = glfwGetKey(m_window->getHandle(), GLFW_KEY_O) == GLFW_PRESS;

    // Trigger only on key press (not hold) while Ctrl is held
    if (ctrlPressed && oKeyPressed && !oKeyWasPressed) {
        openFileDialog();
    }

    // Always update state to handle release properly
    oKeyWasPressed = oKeyPressed;
}

void Application::openFileDialog() {
    auto path = FileDialog::openModel();

    if (path) {
        loadModel(*path);
    }
}

void Application::loadModel(const std::filesystem::path& path) {
    // TODO: Implement actual model loading when parsers are available (Stories 3-1, 3-2, 3-3)
    // For now, just log the selected file
    Logger::info("Loader", "File selected: " + path.string());
    Logger::warn("Loader", "Model loading not yet implemented - waiting for parser integration");
}

} // namespace Vecna::Core
