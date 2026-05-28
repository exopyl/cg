#include "cgre2/Logger.hpp"

#include <iostream>
#include <mutex>
#include <vector>

namespace cgre2 {

Logger::Level Logger::s_minLevel = Level::Info;

namespace {
// Meyers singletons — avoids static init-order issues with the header's
// statics and keeps the sink list + its guard together.
std::mutex& sinkMutex() {
    static std::mutex m;
    return m;
}
std::vector<Logger::Sink>& sinks() {
    static std::vector<Logger::Sink> s;
    return s;
}
} // namespace

void Logger::setMinLevel(Level level) {
    s_minLevel = level;
}

Logger::Level Logger::getMinLevel() {
    return s_minLevel;
}

void Logger::addSink(Sink sink) {
    std::lock_guard<std::mutex> lock(sinkMutex());
    sinks().push_back(std::move(sink));
}

void Logger::clearSinks() {
    std::lock_guard<std::mutex> lock(sinkMutex());
    sinks().clear();
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

    // Fan out to registered sinks (e.g. a TCP log stream). Held under the
    // lock so a concurrent clearSinks() can't free a sink mid-call; sinks
    // must therefore not call back into Logger (would deadlock).
    std::lock_guard<std::mutex> lock(sinkMutex());
    for (const auto& sink : sinks()) {
        if (sink)
            sink(level, module, message);
    }
}

} // namespace cgre2
