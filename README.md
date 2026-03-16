# Vortek

**Vortek** is a lightweight, high-performance **in-memory key-value store** written in modern C++20.

It implements a Redis-compatible wire protocol (RESP2) with a focus on clean architecture, thread safety, async I/O, and minimal dependencies — built as a portfolio/learning project.

## Features

- **RESP2 protocol** — fully compatible with `redis-cli` and any Redis client library
- **Core commands** — `SET`, `GET`, `DEL`, `EXISTS`, `PING`, `INCR`, `INCRBY`, `DECR`, `DECRBY`, `EXPIRE`, `TTL`, `PERSIST`
- **SET options** — `EX` (seconds TTL), `PX` (milliseconds TTL), `NX` (only if absent), `XX` (only if present)
- **Thread-safe** — `std::shared_mutex` for concurrent reads, exclusive writes
- **Key expiry** — lazy expiry on every read + background sweep every 100 ms (`std::jthread`)
- **AOF persistence** — write commands are appended to a file and replayed on startup
- **Async networking** — standalone Asio, no Boost required
- **Structured logging** — spdlog with configurable log level

## Versioning

Release history is tracked in [`CHANGELOG.md`](./CHANGELOG.md).

## Tech Stack

| Concern | Library / Feature |
|---|---|
| Language | C++20 |
| Networking | [Asio](https://think-async.com/Asio/) (standalone, no Boost) |
| Concurrency | `std::jthread`, `std::shared_mutex`, `std::stop_token` |
| Logging | [spdlog](https://github.com/gabime/spdlog) |
| Testing | [Catch2 v3](https://github.com/catchorg/Catch2) |
| Build | CMake 3.22+ with FetchContent (no manual installs) |

## Requirements

- C++20 compiler: GCC 11+, Clang 13+, or MSVC 19.29+
- CMake 3.22+
- Internet access on first build (FetchContent downloads Asio, Catch2, spdlog)

## Build & Run

```bash
# Clone
git clone https://github.com/YOUR_USERNAME/vortek.git
cd vortek

# Configure + build (Debug)
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel

# Run with defaults (port 6379, AOF enabled → vortek.aof)
./build/vortek

# Run with custom options
./build/vortek --port 6380 --aof /tmp/mystore.aof --log-level debug

# Disable AOF
./build/vortek --no-aof

# Help
./build/vortek --help
```

## CLI Options

| Flag | Default | Description |
|---|---|---|
| `--port <n>` | `6379` | TCP port to listen on |
| `--aof <path>` | `vortek.aof` | Enable AOF and set the file path |
| `--no-aof` | — | Disable AOF persistence |
| `--requirepass <pw>` | empty | Require clients to authenticate via `AUTH` |
| `--log-level <lvl>` | `info` | `debug` \| `info` \| `warn` \| `error` |
| `--log-format <fmt>` | `plain` | `plain` \| `json` structured logs |
| `--max-request-bytes <n>` | `1048576` | Maximum buffered request size per connection |
| `--idle-timeout-seconds <n>` | `300` | Close a connection after `n` seconds of inactivity |
| `--max-clients <n>` | `10000` | Maximum concurrent client connections |
| `--max-pending-write-bytes <n>` | `1048576` | Maximum buffered reply bytes per connection |
| `--help` | — | Print usage and exit |

## Running Tests

```bash
cmake --build build --parallel
cd build && ctest --output-on-failure
```

## Smoke Test

With a server running, exercise all commands end-to-end:

```bash
./scripts/smoke_test.sh
# or against a non-default host/port:
./scripts/smoke_test.sh 127.0.0.1 6380
```

## Supported Commands

| Command | Syntax | Description |
|---|---|---|
| `PING` | `PING [msg]` | Returns `PONG` or echoes `msg` |
| `AUTH` | `AUTH password` | Authenticate when `--requirepass` is configured |
| `HEALTH` | `HEALTH` | Liveness check (`OK`) |
| `READY` | `READY` | Readiness check (`READY` or error) |
| `METRICS` | `METRICS` | Prometheus-style metrics payload |
| `SET` | `SET key value [EX s] [PX ms] [NX\|XX]` | Set a key |
| `GET` | `GET key` | Get a key's value |
| `DEL` | `DEL key [key ...]` | Delete one or more keys |
| `EXISTS` | `EXISTS key [key ...]` | Count how many keys exist |
| `EXPIRE` | `EXPIRE key seconds` | Set a TTL in seconds |
| `TTL` | `TTL key` | Get remaining TTL (`-1` = no expiry, `-2` = missing) |
| `PERSIST` | `PERSIST key` | Remove TTL from a key |
| `INCR` | `INCR key` | Increment integer value by 1 |
| `DECR` | `DECR key` | Decrement integer value by 1 |
| `INCRBY` | `INCRBY key n` | Increment by `n` |
| `DECRBY` | `DECRBY key n` | Decrement by `n` |

## Compatibility

See [`COMPATIBILITY.md`](./COMPATIBILITY.md) for protocol/command parity status and known gaps.

## Packaging

### Docker

```bash
docker build -t vortek:0.1.0 .
docker run --rm -p 6379:6379 vortek:0.1.0
```

### Debian package (`.deb`)

```bash
./scripts/package_deb.sh
# output is generated in build/ as a .deb package
```

## Architecture

```
src/
├── main.cpp                  — entry point, CLI parsing
├── server/
│   ├── server.hpp/cpp        — TCP acceptor, io_context
│   └── connection.hpp/cpp    — per-client async read/write loop
├── protocol/
│   ├── resp_types.hpp        — RespValue variant (null, string, error, int, bulk, array)
│   ├── resp_parser.hpp/cpp   — incremental RESP2 parser
│   └── resp_serializer.hpp   — serialize RespValue → wire bytes
├── core/
│   ├── value.hpp             — Value type (string, extensible to list/set/hash)
│   └── kv_store.hpp/cpp      — thread-safe store with background expiry
├── commands/
│   ├── command.hpp           — Command struct + parse_command()
│   ├── dispatcher.hpp/cpp    — routes command name → handler
│   └── handlers/
│       ├── generic_cmds      — PING, DEL, EXISTS, EXPIRE, TTL, PERSIST
│       └── string_cmds       — SET, GET, INCR, INCRBY, DECR, DECRBY
├── persistence/
│   └── aof_persistence.hpp/cpp — AOF write + replay
└── utils/
    ├── logger.hpp            — spdlog wrapper
    ├── config.hpp            — runtime configuration struct
    ├── error.hpp             — typed error codes
    └── byte_view.hpp         — std::span<const std::byte> alias
```

## Why Vortek?

Vortek is not trying to replace Redis in production. The goals are:

- Demonstrate modern C++ in a real-world server context (async I/O, concurrency, protocol parsing)
- Keep the codebase readable, modular, and easy to extend
- Serve as a portfolio piece showing systems programming skills
