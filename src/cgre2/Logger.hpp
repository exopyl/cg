#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace cgre2 {

class Logger {
public:
    enum class Level {
        Debug,
        Info,
        Warn,
        Error
    };

    /// Extra output callback, invoked for every message that passes the
    /// min-level filter, after the built-in console output. May fire from
    /// any thread, so a sink must be thread-safe (e.g. marshal to its own
    /// thread before touching non-thread-safe resources). The string_views
    /// are only valid for the duration of the call — copy if retaining.
    using Sink = std::function<void(Level level,
                                    std::string_view module,
                                    std::string_view message)>;

    static void setMinLevel(Level level);
    static Level getMinLevel();

    /// Register an additional sink. No per-sink removal — call clearSinks()
    /// to drop all (e.g. before destroying an object a sink captured).
    static void addSink(Sink sink);
    static void clearSinks();

    static void debug(std::string_view module, std::string_view message);
    static void info(std::string_view module, std::string_view message);
    static void warn(std::string_view module, std::string_view message);
    static void error(std::string_view module, std::string_view message);

    static void log(Level level, std::string_view module, std::string_view message);

private:
    static Level s_minLevel;
};

} // namespace cgre2
