# Vortek — Development Progress

## Status Legend
- [ ] Not started
- [~] In progress
- [x] Done

---

## Phase 1 — Foundation

- [~] CMake build system setup
  - [x] Project structure & folder layout
  - [x] C++20 configuration
  - [x] FetchContent for Asio (standalone)
  - [x] FetchContent for Catch2 v3
  - [x] `vortek_lib` static library target
  - [x] Test executable wired up
  - [ ] Verify clean build end-to-end

---

## Phase 2 — Protocol (RESP2)

- [x] `resp_types.hpp` — RespValue variant type (simple string, error, integer, bulk string, array, null)
- [x] `resp_parser.hpp/cpp` — incremental RESP2 parser
- [x] `resp_serializer.hpp` — serialize RespValue → wire bytes
- [x] Tests: `resp_parser_tests.cpp`

---

## Phase 3 — Core KV Store

- [x] `value.hpp` — Value type (string + future types)
- [x] `kv_store.hpp/cpp` — thread-safe store with `std::shared_mutex`
  - [x] `set`, `get`, `del`, `exists`
  - [x] TTL / expiry metadata per key
  - [x] Background expiry thread (`std::jthread`)
- [x] Tests: `kv_store_tests.cpp`

---

## Phase 4 — Command Layer

- [x] `command.hpp` — Command struct (name + args)
- [x] `dispatcher.hpp/cpp` — route command name → handler
- [x] Handlers
  - [x] `generic_cmds` — `PING`, `DEL`, `EXISTS`, `EXPIRE`, `TTL`, `PERSIST`
  - [x] `string_cmds` — `SET`, `GET`, `INCR`, `INCRBY`, `DECR`, `DECRBY`

---

## Phase 5 — Networking

- [x] `connection.hpp/cpp` — async read/write per client (Asio)
- [x] `server.hpp/cpp` — TCP acceptor, connection lifecycle
- [x] `main.cpp` — wire everything together, listen on port 6379

---

## Phase 6 — Persistence (AOF)

- [ ] `aof_persistence.hpp/cpp`
  - [ ] Append write commands to file
  - [ ] Replay AOF on startup

---

## Phase 7 — Polish

- [ ] Logger (`utils/logger.hpp`) — structured logging via spdlog or std::print
- [ ] Config (`utils/config.hpp`) — port, AOF path, log level
- [ ] `error.hpp` — typed error codes / result type
- [ ] `byte_view.hpp` — non-owning byte span helper
- [ ] README build/run instructions verified
- [ ] Benchmark / smoke test with `redis-cli`

---

## Future (post-MVP)

- [ ] PUB/SUB channels
- [ ] List, Set, Hash data types
- [ ] TOML/JSON config file
- [ ] INFO command
- [ ] Benchmark suite
