#pragma once

namespace Vecna::Core {

/// Singleton managing GLFW initialization and termination.
/// Ensures glfwInit() is called once and glfwTerminate() is called
/// only when all Window instances are destroyed.
class GLFWContext {
public:
    /// Get the singleton instance, initializing GLFW if needed.
    /// @throws std::runtime_error if GLFW initialization fails.
    static GLFWContext& instance();

    /// Increment reference count (called by Window constructor).
    void addRef();

    /// Decrement reference count (called by Window destructor).
    /// Terminates GLFW when count reaches zero.
    void release();

    // Non-copyable, non-movable
    GLFWContext(const GLFWContext&) = delete;
    GLFWContext& operator=(const GLFWContext&) = delete;
    GLFWContext(GLFWContext&&) = delete;
    GLFWContext& operator=(GLFWContext&&) = delete;

private:
    GLFWContext();
    ~GLFWContext();

    int m_refCount = 0;
    bool m_initialized = false;
};

} // namespace Vecna::Core
