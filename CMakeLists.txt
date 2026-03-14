# Vortek - High-performance in-memory key-value store
# CMakeLists.txt

cmake_minimum_required(VERSION 3.22)
project(Vortek
    VERSION 0.1.0
    DESCRIPTION "Lightweight, modern C++ in-memory key-value store (Redis-inspired)"
    LANGUAGES CXX
)

# Enforce C++20 - we want concepts, ranges, coroutines, jthread, etc.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Helpful for IDEs (clangd, YouCompleteMe, etc.)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Good set of warnings — scoped to our own targets via an INTERFACE library
# so that third-party deps fetched by FetchContent are not affected.
add_library(vortek_warnings INTERFACE)
if (MSVC)
    target_compile_options(vortek_warnings INTERFACE /W4 /permissive- /WX)
else()
    target_compile_options(vortek_warnings INTERFACE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Wunused -Woverloaded-virtual
        -Wconversion -Wsign-conversion
        -Werror=return-type
    )
endif()

# Dependencies
find_package(Threads REQUIRED)

include(FetchContent)

# Asio (header-only, standalone — no Boost)
FetchContent_Declare(
    asio
    GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
    GIT_TAG        asio-1-30-2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(asio)

add_library(asio::asio INTERFACE IMPORTED)
target_include_directories(asio::asio SYSTEM INTERFACE ${asio_SOURCE_DIR}/asio/include)
target_compile_definitions(asio::asio INTERFACE ASIO_STANDALONE)
target_link_libraries(asio::asio INTERFACE Threads::Threads)

# Catch2
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# spdlog (header-only)
FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG        v1.14.1
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(spdlog)
# spdlog::spdlog_header_only is an ALIAS and can't be modified.
# Wrap it in our own interface target so headers are treated as SYSTEM
# (suppresses warnings from spdlog internals in our translation units).
add_library(vortek_spdlog INTERFACE)
target_include_directories(vortek_spdlog SYSTEM INTERFACE ${spdlog_SOURCE_DIR}/include)
target_compile_definitions(vortek_spdlog INTERFACE SPDLOG_HEADER_ONLY)

# toml++ (header-only)
FetchContent_Declare(
    tomlplusplus
    GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
    GIT_TAG        v3.4.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(tomlplusplus)
add_library(vortek_toml INTERFACE)
target_include_directories(vortek_toml SYSTEM INTERFACE ${tomlplusplus_SOURCE_DIR}/include)

# Core library — shared between the main executable and tests
add_library(vortek_lib STATIC
    src/server/server.cpp
    src/server/connection.cpp
    src/protocol/resp_parser.cpp
    src/core/kv_store.cpp
    src/commands/dispatcher.cpp
    src/commands/handlers/generic_cmds.cpp
    src/commands/handlers/string_cmds.cpp
    src/persistence/aof_persistence.cpp
    src/utils/config_loader.cpp
)

target_include_directories(vortek_lib PUBLIC ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(vortek_lib
    PUBLIC
        Threads::Threads
        asio::asio
        vortek_spdlog
        vortek_toml
    PRIVATE
        vortek_warnings
)

# Main executable
add_executable(vortek src/main.cpp)

target_link_libraries(vortek
    PRIVATE
        vortek_lib
        vortek_warnings
)

# Tests
enable_testing()
add_subdirectory(tests)
