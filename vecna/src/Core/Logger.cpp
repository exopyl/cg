#include "Vecna/Core/Logger.hpp"

#include <iostream>

namespace Vecna::Core {

Logger::Level Logger::s_minLevel = Level::Info;

void Logger::setMinLevel(Level level) {
    s_minLevel = level;
}

Logger::Level Logger::getMinLevel() {
    return s_minLevel;
}

void Logger::debug(std::string_view module, std::string_view message) {
    log(Level::Debug, module, message);
}

void Logger::info(std::string_view module, std::string_view message) {
    log(Level::Info, module, message);
}

void Logger::warn(std::string_view module, std::string_view message) {
    log(Level::Warn, module, message);
}

void Logger::error(std::string_view module, std::string_view message) {
    log(Level::Error, module, message);
}

void Logger::log(Level level, std::string_view module, std::string_view message) {
    if (level < s_minLevel) {
        return;
    }

    // Use stderr for warnings and errors, stdout for others
    std::ostream& output = (level >= Level::Warn) ? std::cerr : std::cout;

    // Format: [MODULE] Message (per architecture.md specification)
    output << "[" << module << "] " << message << std::endl;
}

} // namespace Vecna::Core
