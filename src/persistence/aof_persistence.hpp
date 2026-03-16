#pragma once

#include "commands/command.hpp"
#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"

#include <cstddef>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>

namespace vortek {

// Append-Only File persistence.
//
// Write path: every mutating command is serialized as a RESP array and
// appended to the AOF file, protected by a mutex for multi-connection safety.
//
// Read path: on startup, replay() re-feeds the file through the RESP parser
// and re-dispatches each command against the store to restore state.
class AofPersistence {
public:
    explicit AofPersistence(std::string path);
    ~AofPersistence();

    AofPersistence(const AofPersistence&)            = delete;
    AofPersistence& operator=(const AofPersistence&) = delete;

    // Returns true if the file was opened successfully.
    bool is_open() const noexcept;

    // Append a write command to the AOF file.
    // Thread-safe: multiple connections may call this concurrently.
    void log(const Command& cmd);

    // Replay all commands stored in the AOF file.
    // Returns the number of commands successfully replayed.
    // Call this once at startup, before accepting client connections.
    std::size_t replay(const Dispatcher& dispatcher, KvStore& store);

    // Returns true if the given command name mutates store state.
    static bool is_write_command(std::string_view name) noexcept;

private:
    bool reopen_append_stream();
    bool truncate_to(std::size_t bytes);

    std::string   path_;
    std::ofstream file_;
    std::mutex    mutex_;
};

}  // namespace vortek
