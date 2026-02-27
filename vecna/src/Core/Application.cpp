#include "Vecna/Core/Application.hpp"
#include "Vecna/Core/FileDialog.hpp"
#include "Vecna/Core/Window.hpp"
#include "Vecna/Core/Logger.hpp"
#include "Vecna/Renderer/Buffer.hpp"
#include "Vecna/Renderer/Pipeline.hpp"
#include "Vecna/Renderer/Swapchain.hpp"
#include "Vecna/Renderer/VulkanDevice.hpp"
#include "Vecna/Renderer/VulkanInstance.hpp"
#include "Vecna/Scene/Trackball.hpp"
#include "Vecna/UI/ImGuiRenderer.hpp"
#ifdef _MSC_VER
#pragma warning(push, 0)
#endif
#include "src/cgmath/TCamera.h"
#include "src/cgmath/TMatrix4.h"
#include "src/cgmesh/mesh.h"
#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <GLFW/glfw3.h>
// ImGui header needed only for WantCaptureMouse check in GLFW callbacks
// (known technical debt — abstracting input capture is out of scope for Story 5-1)
#include <imgui.h>

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

// Zoom constants (Story 4-3)
static constexpr float ZOOM_FACTOR = 0.1f;     // 10% per scroll notch
static constexpr float MIN_DISTANCE = 0.01f;    // Minimum camera distance
static constexpr float MAX_DISTANCE = 10000.0f;  // Maximum camera distance

Application::Application() {
    Logger::info("Core", "Application starting");

    // Create window with default configuration (1280x720, "Vecna", resizable)
    // Window must be created BEFORE VulkanInstance (GLFW must be initialized for extensions)
    m_camera = std::make_unique<Cameraf>();
    // Set initial clipping planes consistent with default camera distance (Z=3)
    m_camera->SetClippingPlanes(m_cameraDistance * 0.01f, m_cameraDistance * 10.0f);
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

    // Initialize trackball rotation (Story 4-2)
    // Callbacks registered BEFORE ImGui so ImGui chains to them (ImGui calls prev callbacks)
    m_trackball = std::make_unique<Scene::Trackball>();
    auto extent = m_swapchain->getExtent();
    m_trackball->setDimensions(static_cast<int>(extent.width), static_cast<int>(extent.height));
    // Application owns the GLFW user pointer (all callbacks route through Application)
    glfwSetWindowUserPointer(m_window->getHandle(), this);
    glfwSetFramebufferSizeCallback(m_window->getHandle(), framebufferResizeCallback);
    glfwSetMouseButtonCallback(m_window->getHandle(), mouseButtonCallback);
    glfwSetCursorPosCallback(m_window->getHandle(), cursorPosCallback);
    glfwSetScrollCallback(m_window->getHandle(), scrollCallback);
    glfwSetDropCallback(m_window->getHandle(), dropCallback);

    // Initialize UI renderer via abstraction (Story 5-1)
    // ImGui_ImplGlfw_InitForVulkan with install_callbacks=true chains to our callbacks above
    auto imguiRenderer = std::make_unique<UI::ImGuiRenderer>(
        *m_vulkanInstance, *m_vulkanDevice, *m_swapchain, *m_window);
    imguiRenderer->setOnFileOpen([this]() { openFileDialog(); });
    m_uiRenderer = std::move(imguiRenderer);
    m_uiRenderer->init();

    Logger::info("Core", "Application initialized");
}

Application::~Application() {
    Logger::info("Core", "Application shutting down");

    // Wait for GPU to finish before cleanup
    if (m_vulkanDevice) {
        vkDeviceWaitIdle(m_vulkanDevice->getDevice());
    }

    // Shutdown UI renderer before destroying Vulkan resources (Story 5-1)
    m_uiRenderer.reset();

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

    while (!m_window->shouldClose()) {
        m_window->pollEvents();

        // Handle keyboard shortcuts (Story 3-4)
        handleKeyboardShortcuts();

        // Process deferred model load (set by Ctrl+O or ImGui menu callback)
        // Must happen outside drawFrame() to avoid destroying buffers mid-render
        if (m_pendingModelPath) {
            loadModel(*m_pendingModelPath);
            m_pendingModelPath.reset();
        }

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
        m_uiRenderer->onSwapchainRecreated();
        auto newExtent = m_swapchain->getExtent();
        m_trackball->setDimensions(static_cast<int>(newExtent.width), static_cast<int>(newExtent.height));
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
        m_uiRenderer->onSwapchainRecreated();
        auto newExtent = m_swapchain->getExtent();
        m_trackball->setDimensions(static_cast<int>(newExtent.width), static_cast<int>(newExtent.height));
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
    // Model rotation from trackball (Story 4-2) — always centered on model origin.
    // Pan offset is handled entirely in the view matrix (camera position/target).
    // This keeps pan direction always aligned with screen axes regardless of rotation.
    Matrix4f model;
    std::copy(m_trackball->getTransform(), m_trackball->getTransform() + 16, model.data());

    float aspect = viewport.width / viewport.height;
    Matrix4f view = m_camera->GetViewMatrix();
    Matrix4f projection = m_camera->GetProjectionMatrix(aspect);

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

    // Render UI overlay via abstraction (Story 5-1)
    m_uiRenderer->beginFrame();
    m_uiRenderer->render(commandBuffer);

    vkCmdEndRenderPass(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to record command buffer");
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
        m_pendingModelPath = *path;
    }
}

void Application::loadModel(const std::filesystem::path& path) {
    Logger::info("Loader", "Loading model: " + path.filename().string());

    // Load mesh using cgmesh
    Mesh mesh;
    int rc = mesh.load(path.string().c_str());
    if (rc != 0) {
        Logger::error("Loader", "Failed to load model: " + path.string());
        showUIError("Impossible de charger le fichier : " + path.filename().string());
        return;
    }

    Logger::info("Loader", "Parsed " + std::to_string(mesh.m_nVertices) + " vertices, " +
                 std::to_string(mesh.m_nFaces) + " faces");

    // Center model at origin using bounding box center
    mesh.computebbox();
    float bboxCenter[3];
    if (!mesh.bbox().GetCenter(bboxCenter)) {
        Logger::error("Loader", "Model has empty bounding box");
        showUIError("Le modele est vide ou invalide");
        return;
    }
    float diagonal = mesh.bbox().GetDiagonalLength();

    // Capture original bounding box BEFORE centering (Story 5-3)
    m_modelInfo.filename = path.filename().string();
    mesh.bbox().GetMinMax(m_modelInfo.bboxMin, m_modelInfo.bboxMax);
    m_modelInfo.diagonal = diagonal;

    mesh.translate(-bboxCenter[0], -bboxCenter[1], -bboxCenter[2]);

    // Adapt camera and clipping planes to model size
    // Guard against degenerate models (points, lines) where diagonal is zero or near-zero
    static constexpr float MIN_DIAGONAL = 1e-6f;
    if (diagonal < MIN_DIAGONAL) {
        diagonal = 1.0f;
    }
    static constexpr float VIEW_MARGIN = 1.5f;  // Extra margin so model doesn't fill the entire frustum
    float cameraDistance = (diagonal / (2.0f * std::tan(m_camera->GetFov() / 2.0f))) * VIEW_MARGIN;
    m_cameraDistance = cameraDistance;
    m_camera->SetTarget(0.0f, 0.0f, 0.0f);
    m_camera->SetPosition(0.0f, 0.0f, m_cameraDistance);
    m_camera->SetClippingPlanes(m_cameraDistance * 0.01f, m_cameraDistance * 10.0f);

    Logger::info("Loader", "Centered model (diagonal: " + std::to_string(diagonal) +
                 ", camera distance: " + std::to_string(cameraDistance) + ")");

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
        showUIError(std::to_string(skippedFaces) + " faces ignorees (indices invalides)");
    }

    if (indices.empty()) {
        Logger::error("Loader", "Model has no valid faces");
        showUIError("Le modele ne contient aucune face valide");
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

    // Capture final vertex/triangle counts AFTER welding (Story 5-3)
    m_modelInfo.vertexCount = static_cast<uint32_t>(vertices.size());
    m_modelInfo.triangleCount = static_cast<uint32_t>(indices.size() / 3);
    m_modelInfo.computeDerived();

    // Wait for GPU idle before replacing buffers
    vkDeviceWaitIdle(m_vulkanDevice->getDevice());

    // Destroy old buffers and create new ones
    m_indexBuffer.reset();
    m_vertexBuffer.reset();

    m_vertexBuffer = std::make_unique<Renderer::VertexBuffer>(*m_vulkanDevice, vertices);
    m_indexBuffer = std::make_unique<Renderer::IndexBuffer>(*m_vulkanDevice, indices);

    // Transmit model info to UI for display (Story 5-3)
    if (auto* imgui = dynamic_cast<UI::ImGuiRenderer*>(m_uiRenderer.get())) {
        imgui->setModelInfo(m_modelInfo);
    }

    Logger::info("Loader", "Model loaded successfully");
}

void Application::showUIError(const std::string& message) {
    if (auto* imgui = dynamic_cast<UI::ImGuiRenderer*>(m_uiRenderer.get())) {
        imgui->showErrorMessage(message);
    }
}

// ============================================================================
// GLFW callbacks (routed via glfwSetWindowUserPointer → Application*)
// ============================================================================

void Application::framebufferResizeCallback(GLFWwindow* window, int width, int height) {
    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    app->m_window->onFramebufferResize(width, height);
}

void Application::mouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    // Let ImGui handle mouse if it wants it
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));
    double xpos = 0;
    double ypos = 0;
    glfwGetCursorPos(window, &xpos, &ypos);

    // Pan: middle mouse button OR Shift+left click (Story 4-4)
    bool isPanButton = (button == GLFW_MOUSE_BUTTON_MIDDLE) ||
                       (button == GLFW_MOUSE_BUTTON_LEFT && (mods & GLFW_MOD_SHIFT));
    if (isPanButton) {
        if (action == GLFW_PRESS) {
            app->m_panning = true;
            app->m_lastPanX = xpos;
            app->m_lastPanY = ypos;
        } else {
            app->m_panning = false;
        }
        return;  // Don't forward pan clicks to trackball
    }

    // Stop panning if left button released without Shift (edge case: Shift released mid-drag)
    // Don't forward to trackball — this release belongs to the pan gesture
    if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE && app->m_panning) {
        app->m_panning = false;
        return;
    }

    app->m_trackball->onMousePress(button, action == GLFW_PRESS,
                                   static_cast<int>(xpos), static_cast<int>(ypos));
}

void Application::cursorPosCallback(GLFWwindow* window, double xpos, double ypos) {
    // Let ImGui handle mouse if it wants it
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    // Pan movement (Story 4-4): translate camera + target in screen plane
    if (app->m_panning) {
        double dx = xpos - app->m_lastPanX;
        double dy = ypos - app->m_lastPanY;
        app->m_lastPanX = xpos;
        app->m_lastPanY = ypos;

        // Scale pan speed proportional to camera distance and inversely to window height
        // This gives consistent feel: 1 pixel of drag ≈ same world-space distance at any zoom
        // Using height for both axes (pixels are square on modern displays)
        int winHeight = 0;
        glfwGetWindowSize(window, nullptr, &winHeight);
        if (winHeight <= 0) {
            return;
        }
        float panScale = app->m_cameraDistance / static_cast<float>(winHeight);

        // Pan in world X/Y — valid because camera is always aligned to Z axis
        // (rotation is in the model matrix via trackball, not in the view matrix).
        // Screen X → world X (negated), Screen Y → world Y (inverted: screen down = +dy)
        float offsetX = -static_cast<float>(dx) * panScale;
        float offsetY = static_cast<float>(dy) * panScale;

        // Move both position and target by the same offset so the view direction stays constant
        auto pos = app->m_camera->GetPosition();
        auto target = app->m_camera->GetTarget();
        app->m_camera->SetPosition(pos[0] + offsetX, pos[1] + offsetY, pos[2]);
        app->m_camera->SetTarget(target[0] + offsetX, target[1] + offsetY, target[2]);
        return;
    }

    app->m_trackball->onMouseMove(static_cast<int>(xpos), static_cast<int>(ypos));
}

void Application::scrollCallback(GLFWwindow* window, double /*xoffset*/, double yoffset) {
    // Let ImGui handle scroll if it wants it
    if (ImGui::GetIO().WantCaptureMouse) {
        return;
    }

    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    // Multiplicative zoom using authoritative m_cameraDistance
    float scale = 1.0f - static_cast<float>(yoffset) * ZOOM_FACTOR;
    // Cap scale factor to prevent negative distance or extreme jumps (high-res scroll wheels)
    scale = std::clamp(scale, 0.1f, 10.0f);
    float oldDistance = app->m_cameraDistance;
    app->m_cameraDistance = std::clamp(app->m_cameraDistance * scale, MIN_DISTANCE, MAX_DISTANCE);

    // Zoom along the view direction (target→position), preserving pan offset (Story 4-4)
    auto pos = app->m_camera->GetPosition();
    auto target = app->m_camera->GetTarget();
    float ratio = app->m_cameraDistance / oldDistance;
    app->m_camera->SetPosition(target[0] + (pos[0] - target[0]) * ratio,
                               target[1] + (pos[1] - target[1]) * ratio,
                               target[2] + (pos[2] - target[2]) * ratio);
    app->m_camera->SetClippingPlanes(app->m_cameraDistance * 0.01f, app->m_cameraDistance * 10.0f);
}

void Application::dropCallback(GLFWwindow* window, int count, const char** paths) {
    if (count < 1) {
        return;
    }

    auto* app = static_cast<Application*>(glfwGetWindowUserPointer(window));

    std::filesystem::path filePath(paths[0]);
    std::string ext = filePath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext != ".obj" && ext != ".stl") {
        app->showUIError("Format non supporte : " + filePath.extension().string());
        return;
    }

    app->m_pendingModelPath = filePath;
}

} // namespace Vecna::Core
