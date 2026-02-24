#pragma once

#include <string>
#include <string_view>

namespace Vecna::Core {

class Logger {
public:
    enum class Level {
        Debug,
        Info,
        Warn,
        Error
    };

    static void setMinLevel(Level level);
    static Level getMinLevel();

    static void debug(std::string_view module, std::string_view message);
    static void info(std::string_view module, std::string_view message);
    static void warn(std::string_view module, std::string_view message);
    static void error(std::string_view module, std::string_view message);

    static void log(Level level, std::string_view module, std::string_view message);

private:
    static Level s_minLevel;
};

} // namespace Vecna::Core
