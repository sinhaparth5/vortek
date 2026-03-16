#pragma once

#include <spdlog/spdlog.h>

#include <string_view>

namespace vortek::log {

inline void debug(std::string_view msg) { spdlog::debug("{}", msg); }
inline void info(std::string_view msg)  { spdlog::info("{}", msg);  }
inline void warn(std::string_view msg)  { spdlog::warn("{}", msg);  }
inline void error(std::string_view msg) { spdlog::error("{}", msg); }

// Configure log rendering format.
// plain: human-readable text
// json:  one JSON object per line (message should avoid raw quotes)
inline void set_format(std::string_view format) {
    if (format == "json") {
        spdlog::set_pattern(
            "{\"ts\":\"%Y-%m-%dT%H:%M:%S.%e\",\"level\":\"%l\",\"msg\":\"%v\"}");
    } else {
        spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    }
}

// Set the global log level from a string (debug | info | warn | error).
// Unknown strings default to info.
inline void set_level(std::string_view level) {
    if      (level == "debug") spdlog::set_level(spdlog::level::debug);
    else if (level == "warn")  spdlog::set_level(spdlog::level::warn);
    else if (level == "error") spdlog::set_level(spdlog::level::err);
    else                       spdlog::set_level(spdlog::level::info);
}

}  // namespace vortek::log
