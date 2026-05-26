#include "Vecna/Core/GLFWContext.hpp"
#include "cgre2/Logger.hpp"

#include <GLFW/glfw3.h>

#include <stdexcept>

namespace Vecna::Core {

GLFWContext& GLFWContext::instance() {
    static GLFWContext instance;
    return instance;
}

GLFWContext::GLFWContext() = default;

GLFWContext::~GLFWContext() {
    if (m_initialized) {
        glfwTerminate();
        cgre2::Logger::info("Core", "GLFW terminated");
    }
}

void GLFWContext::addRef() {
    if (!m_initialized) {
        if (glfwInit() == GLFW_FALSE) {
            cgre2::Logger::error("Core", "Failed to initialize GLFW");
            throw std::runtime_error("Failed to initialize GLFW");
        }
        m_initialized = true;
        cgre2::Logger::info("Core", "GLFW initialized");
    }
    ++m_refCount;
}

void GLFWContext::release() {
    if (m_refCount > 0) {
        --m_refCount;
    }
    // Note: GLFW termination happens in destructor (program exit)
    // This ensures GLFW stays alive for the entire application lifetime
}

} // namespace Vecna::Core
