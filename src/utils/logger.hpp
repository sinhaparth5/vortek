#pragma once

#include <cstdio>
#include <string_view>

// Minimal console logger. Will be replaced by spdlog in Phase 7.
namespace vortek::log {

inline void info(std::string_view msg) {
    std::printf("[INFO]  %.*s\n", static_cast<int>(msg.size()), msg.data());
}

inline void warn(std::string_view msg) {
    std::printf("[WARN]  %.*s\n", static_cast<int>(msg.size()), msg.data());
}

inline void error(std::string_view msg) {
    std::fprintf(stderr, "[ERROR] %.*s\n", static_cast<int>(msg.size()), msg.data());
}

}  // namespace vortek::log
