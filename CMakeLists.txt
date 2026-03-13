# Vortek - High-performance in-memory key-value store
# CMakeLists.txt

cmake_minimum_required(VERSION 3.22)
project(Vortek
    VERSION 0.1.0
    DESCRIPTION "Lightweight, modern C++ in-memory key-value store (Redis-inspired)"
    LANGUAGES CXX
)

# Enfore C++20 - we want concepts, ranges, coroutines, jthread, etc.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Helpful for IDEs (clangd, YouCompleteMe, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Good set of warnings - catch bugs early

if (MSVC)
    add_compile_options(/W4 /permissive- /WX)
else()
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion
        -Werror=return-type
    )
endif()

# Dependencies
find_package(Threads REQUIRED)
find_package(asio CONFIG REQUIRED)
# Add these later when use them in the codebase
# find_package(spdlog CONFIG REQUIRED)
# find_package(Catch2 CONFIG REQUIRED)

# Main executable
add_executable(vortek)
target_sources(vortek
    PRIVATE
        src/main.cpp
)