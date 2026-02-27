#include "Vecna/Core/Window.hpp"
#include "Vecna/Core/GLFWContext.hpp"
#include "Vecna/Core/Logger.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace Vecna::Core {

Window::Window(const Config& config)
    : m_width(config.width)
    , m_height(config.height)
    , m_title(config.title) {

    // Validate dimensions
    if (m_width < MIN_WIDTH || m_width > MAX_WIDTH) {
        throw std::invalid_argument(
            "Window width must be between " + std::to_string(MIN_WIDTH) +
            " and " + std::to_string(MAX_WIDTH) + ", got " + std::to_string(m_width));
    }
    if (m_height < MIN_HEIGHT || m_height > MAX_HEIGHT) {
        throw std::invalid_argument(
            "Window height must be between " + std::to_string(MIN_HEIGHT) +
            " and " + std::to_string(MAX_HEIGHT) + ", got " + std::to_string(m_height));
    }

    // Initialize GLFW via singleton (throws on failure)
    GLFWContext::instance().addRef();

    // Configure for Vulkan (no OpenGL context)
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, config.resizable ? GLFW_TRUE : GLFW_FALSE);

    // Create window
    m_window = glfwCreateWindow(
        static_cast<int>(m_width),
        static_cast<int>(m_height),
        m_title.c_str(),
        nullptr,
        nullptr
    );

    if (m_window == nullptr) {
        GLFWContext::instance().release();
        Logger::error("Core", "Failed to create GLFW window");
        throw std::runtime_error("Failed to create GLFW window");
    }

    Logger::info("Core", "Window created: " + std::to_string(m_width) + "x" + std::to_string(m_height));
}

Window::~Window() {
    if (m_window != nullptr) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
        Logger::info("Core", "Window destroyed");
    }
    GLFWContext::instance().release();
}

bool Window::shouldClose() const {
    return glfwWindowShouldClose(m_window) != 0;
}

void Window::pollEvents() const {
    glfwPollEvents();
}

void Window::close() {
    glfwSetWindowShouldClose(m_window, GLFW_TRUE);
}

void Window::onFramebufferResize(int width, int height) {
    m_framebufferResized = true;
    m_width = static_cast<uint32_t>(width);
    m_height = static_cast<uint32_t>(height);
    Logger::debug("Core", "Window resized: " + std::to_string(width) + "x" + std::to_string(height));
}

} // namespace Vecna::Core
