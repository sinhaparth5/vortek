# Vortek

**Vortek** is a lightweight, high-performance **in-memory key-value store** written in modern C++20.

It aims to be a clean, understandable, and extensible alternative to Redis — with focus on good software engineering practices, thread safety, async I/O, and minimal dependencies.

Currently in **early development** — many features are still being implemented.

## Features (planned / partially implemented)

- RESP2 protocol support (Redis-compatible wire protocol)
- Core commands:
  - `SET`, `GET`, `DEL`, `EXISTS`, `PING`
  - `INCR`, `INCRBY`, `DECR`, `DECRBY`
  - `EXPIRE`, `TTL`, `PERSIST`
- Thread-safe concurrent access (multiple client connections)
- Background key expiration & eviction
- Append-Only File (AOF) persistence (optional)
- Asynchronous networking using standalone **Asio**
- Clean, modular architecture with unit tests
- CMake-based build system

Future goals (subject to change):

- Basic PUB/SUB channels
- More data types (lists, sets, hashes)
- Configuration file support (TOML/JSON)
- Better monitoring / INFO command
- Benchmark suite & performance numbers

## Why another Redis clone?

Vortek is **not** trying to replace Redis in production at massive scale.

The main goals are:

- Educational / learning project demonstrating modern C++ in real-world server context
- Clean code base that is easy to read, extend, and understand
- Playground for experimenting with concurrency patterns, async I/O, protocol parsing, memory management
- Resume / portfolio piece showing systems programming skills

## Tech Stack

- **Language**: C++20
- **Networking**: standalone [Asio](https://think-async.com/Asio/)
- **Build system**: CMake 3.22+
- **Concurrency**: `std::jthread`, `std::shared_mutex`, `std::stop_token`, etc.
- **Testing**: Catch2 (planned)
- **Logging**: spdlog (optional / planned)
- **No Boost dependency** (standalone Asio only)

## Requirements

- C++20 compliant compiler (GCC 11+, Clang 13+, MSVC 19.29+)
- CMake 3.22+
- Threads support (`pthread` on Linux/macOS)

## Build & Run

```bash
# 1. Clone
git clone https://github.com/YOUR_USERNAME/vortek.git
cd vortek

# 2. Create build directory
mkdir build && cd build

# 3. Configure
cmake .. -DCMAKE_BUILD_TYPE=Release

# 4. Build
cmake --build . --parallel

# 5. Run (default port 6379)
./vortek