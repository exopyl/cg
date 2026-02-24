#include "Vecna/Core/Application.hpp"
#include "Vecna/Core/FileDialog.hpp"
#include "Vecna/Core/Window.hpp"
#include "Vecna/Core/Logger.hpp"
#include "Vecna/Renderer/Buffer.hpp"
#include "Vecna/Renderer/Pipeline.hpp"
#include "Vecna/Renderer/Swapchain.hpp"
#include "Vecna/Renderer/VulkanDevice.hpp"
#include "Vecna/Renderer/VulkanInstance.hpp"
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include "src/cgmath/TMatrix4.h"
#include "src/cgmesh/mesh.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <GLFW/glfw3.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <unordered_map>

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
    // Geometry buffers → Pipeline → Swapchain → VulkanDevice → Surface → VulkanInstance → Window (GLFW)
    // IMPORTANT: Geometry buffers must be destroyed BEFORE VulkanDevice (require VMA allocator)
    // IMPORTANT: Pipeline must be destroyed BEFORE Swapchain (references render pass)
    m_indexBuffer.reset();
    m_vertexBuffer.reset();
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

    m_vertexBuffer = std::make_unique<Renderer::VertexBuffer>(*m_vulkanDevice, vertices);
    m_indexBuffer = std::make_unique<Renderer::IndexBuffer>(*m_vulkanDevice, indices);

    Logger::info("Core", "Cube buffers created (vertex: " +
                 std::to_string(m_vertexBuffer->getSize()) + " bytes, index: " +
                 std::to_string(m_indexBuffer->getSize()) + " bytes)");
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
    if (!m_vertexBuffer || !m_indexBuffer) {
        throw std::runtime_error("Geometry buffers not initialized");
    }

    // Bind vertex buffer
    VkBuffer vertexBuffers[] = {m_vertexBuffer->getBuffer()};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

    // Bind index buffer
    vkCmdBindIndexBuffer(commandBuffer, m_indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);

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
    vkCmdDrawIndexed(commandBuffer, m_indexBuffer->getIndexCount(), 1, 0, 0, 0);

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
    Logger::info("Loader", "Loading model: " + path.filename().string());

    // Load mesh using cgmesh
    Mesh mesh;
    int rc = mesh.load(path.string().c_str());
    if (rc != 0) {
        Logger::error("Loader", "Failed to load model: " + path.string());
        return;
    }

    Logger::info("Loader", "Parsed " + std::to_string(mesh.m_nVertices) + " vertices, " +
                 std::to_string(mesh.m_nFaces) + " faces");

    // Compute normals if not present
    if (mesh.m_pVertexNormals == nullptr) {
        mesh.ComputeNormals();
    }

    // Convert cgmesh data to Vecna vertex/index format
    std::vector<Renderer::Vertex> vertices;
    std::vector<uint32_t> indices;

    // Build vertex array from mesh data
    vertices.resize(mesh.m_nVertices);
    for (unsigned int i = 0; i < mesh.m_nVertices; i++) {
        Renderer::Vertex& v = vertices[i];
        v.position[0] = mesh.m_pVertices[3 * i];
        v.position[1] = mesh.m_pVertices[3 * i + 1];
        v.position[2] = mesh.m_pVertices[3 * i + 2];

        if (mesh.m_pVertexNormals) {
            v.normal[0] = mesh.m_pVertexNormals[3 * i];
            v.normal[1] = mesh.m_pVertexNormals[3 * i + 1];
            v.normal[2] = mesh.m_pVertexNormals[3 * i + 2];
        } else if (mesh.m_pFaceNormals) {
            // Fallback: will be overwritten per-face below
            v.normal[0] = 0.0f;
            v.normal[1] = 1.0f;
            v.normal[2] = 0.0f;
        }

        // Default gray color
        v.color[0] = 0.7f;
        v.color[1] = 0.7f;
        v.color[2] = 0.7f;
    }

    // Build index array, triangulating quads (fan triangulation)
    unsigned int skippedFaces = 0;
    for (unsigned int fi = 0; fi < mesh.m_nFaces; fi++) {
        Face* face = mesh.GetFace(fi);
        unsigned int nv = face->GetNVertices();

        if (nv < 3) {
            continue;
        }

        // Validate all vertex indices for this face before emitting triangles
        bool validFace = true;
        for (unsigned int j = 0; j < nv; j++) {
            int idx = face->GetVertex(j);
            if (idx < 0 || static_cast<unsigned int>(idx) >= mesh.m_nVertices) {
                validFace = false;
                break;
            }
        }
        if (!validFace) {
            skippedFaces++;
            continue;
        }

        // Fan triangulation: for each face with N vertices, emit N-2 triangles
        auto v0 = static_cast<uint32_t>(face->GetVertex(0));
        for (unsigned int j = 1; j + 1 < nv; j++) {
            auto v1 = static_cast<uint32_t>(face->GetVertex(j));
            auto v2 = static_cast<uint32_t>(face->GetVertex(j + 1));
            indices.push_back(v0);
            indices.push_back(v1);
            indices.push_back(v2);
        }
    }

    if (skippedFaces > 0) {
        Logger::warn("Loader", "Skipped " + std::to_string(skippedFaces) +
                     " faces with out-of-bounds vertex indices");
    }

    if (indices.empty()) {
        Logger::error("Loader", "Model has no valid faces");
        return;
    }

    Logger::info("Loader", "Converted to " + std::to_string(vertices.size()) + " vertices, " +
                 std::to_string(indices.size() / 3) + " triangles");

    // STL files have 3 unique vertices per triangle (no sharing).
    // Weld duplicate vertices by position and recompute smooth normals.
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (ext == ".stl") {
        // Spatial hash: quantize positions to merge vertices within epsilon
        struct PositionHash {
            size_t operator()(const std::array<int32_t, 3>& k) const {
                size_t h = 0;
                for (auto v : k) {
                    h ^= std::hash<int32_t>{}(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
                }
                return h;
            }
        };

        constexpr float WELD_SCALE = 1e5f;  // ~0.01mm precision
        std::unordered_map<std::array<int32_t, 3>, uint32_t, PositionHash> posMap;
        std::vector<Renderer::Vertex> weldedVerts;
        std::vector<uint32_t> remap(vertices.size());

        for (size_t i = 0; i < vertices.size(); i++) {
            std::array<int32_t, 3> key = {
                static_cast<int32_t>(std::round(vertices[i].position[0] * WELD_SCALE)),
                static_cast<int32_t>(std::round(vertices[i].position[1] * WELD_SCALE)),
                static_cast<int32_t>(std::round(vertices[i].position[2] * WELD_SCALE))
            };

            auto [it, inserted] = posMap.emplace(key, static_cast<uint32_t>(weldedVerts.size()));
            if (inserted) {
                weldedVerts.push_back(vertices[i]);
            }
            remap[i] = it->second;
        }

        // Remap indices
        for (auto& idx : indices) {
            idx = remap[idx];
        }

        size_t beforeCount = vertices.size();
        vertices = std::move(weldedVerts);

        // Recompute smooth normals (average of adjacent face normals)
        for (auto& v : vertices) {
            v.normal[0] = v.normal[1] = v.normal[2] = 0.0f;
        }
        for (size_t i = 0; i + 2 < indices.size(); i += 3) {
            auto& a = vertices[indices[i]];
            auto& b = vertices[indices[i + 1]];
            auto& c = vertices[indices[i + 2]];

            // Edge vectors
            float e1[3] = {b.position[0] - a.position[0], b.position[1] - a.position[1], b.position[2] - a.position[2]};
            float e2[3] = {c.position[0] - a.position[0], c.position[1] - a.position[1], c.position[2] - a.position[2]};

            // Cross product (face normal, not normalized - area-weighted)
            float fn[3] = {
                e1[1] * e2[2] - e1[2] * e2[1],
                e1[2] * e2[0] - e1[0] * e2[2],
                e1[0] * e2[1] - e1[1] * e2[0]
            };

            // Accumulate to each vertex of the triangle
            for (uint32_t vi : {indices[i], indices[i + 1], indices[i + 2]}) {
                vertices[vi].normal[0] += fn[0];
                vertices[vi].normal[1] += fn[1];
                vertices[vi].normal[2] += fn[2];
            }
        }
        // Normalize
        for (auto& v : vertices) {
            float len = std::sqrt(v.normal[0] * v.normal[0] + v.normal[1] * v.normal[1] + v.normal[2] * v.normal[2]);
            if (len > 1e-8f) {
                v.normal[0] /= len;
                v.normal[1] /= len;
                v.normal[2] /= len;
            }
        }

        Logger::info("Loader", "Welded " + std::to_string(beforeCount) + " -> " +
                     std::to_string(vertices.size()) + " vertices");
    }

    // Wait for GPU idle before replacing buffers
    vkDeviceWaitIdle(m_vulkanDevice->getDevice());

    // Destroy old buffers and create new ones
    m_indexBuffer.reset();
    m_vertexBuffer.reset();

    m_vertexBuffer = std::make_unique<Renderer::VertexBuffer>(*m_vulkanDevice, vertices);
    m_indexBuffer = std::make_unique<Renderer::IndexBuffer>(*m_vulkanDevice, indices);

    Logger::info("Loader", "Model loaded successfully");
}

} // namespace Vecna::Core
