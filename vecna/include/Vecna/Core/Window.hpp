#pragma once

#include <cstdint>
#include <string>

struct GLFWwindow;

namespace Vecna::Core {

class Window {
public:
    // Minimum window dimensions (GLFW and Vulkan constraints)
    static constexpr uint32_t MIN_WIDTH = 64;
    static constexpr uint32_t MIN_HEIGHT = 64;
    static constexpr uint32_t MAX_WIDTH = 16384;
    static constexpr uint32_t MAX_HEIGHT = 16384;

    struct Config {
        uint32_t width = 1280;
        uint32_t height = 720;
        std::string title = "Vecna";
        bool resizable = true;
    };

    /// Create a window with the given configuration.
    /// @param config Window configuration (dimensions, title, resizable).
    /// @throws std::invalid_argument if dimensions are outside valid range.
    /// @throws std::runtime_error if GLFW initialization or window creation fails.
    explicit Window(const Config& config = {});
    ~Window();

    // Non-copyable, non-movable (GLFW window handle)
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&&) = delete;
    Window& operator=(Window&&) = delete;

    [[nodiscard]] bool shouldClose() const;
    void pollEvents() const;
    void close();

    /// Get the underlying GLFW window handle.
    /// @note The caller must NOT destroy this handle - it is owned by the Window.
    /// @note Required for Vulkan surface creation (vkCreateWindowSurface).
    /// @return Raw GLFW window pointer.
    [[nodiscard]] GLFWwindow* getHandle() const { return m_window; }

    [[nodiscard]] uint32_t getWidth() const { return m_width; }
    [[nodiscard]] uint32_t getHeight() const { return m_height; }
    [[nodiscard]] const std::string& getTitle() const { return m_title; }
    [[nodiscard]] bool wasResized() const { return m_framebufferResized; }
    void resetResizedFlag() { m_framebufferResized = false; }

    /// Called by Application when framebuffer is resized (routed via GLFW callback).
    void onFramebufferResize(int width, int height);

private:

    GLFWwindow* m_window = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;
    std::string m_title;
    bool m_framebufferResized = false;
};

} // namespace Vecna::Core
