#include "aof_persistence.hpp"

#include "protocol/resp_parser.hpp"
#include "protocol/resp_serializer.hpp"
#include "utils/logger.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <unordered_set>

namespace vortek {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

AofPersistence::AofPersistence(std::string path)
    : path_(std::move(path))
    , file_(path_, std::ios::app | std::ios::binary) {
    if (!file_.is_open())
        log::error("AOF: could not open file: " + path_);
    else
        log::info("AOF: logging to " + path_);
}

AofPersistence::~AofPersistence() {
    if (file_.is_open())
        file_.flush();
}

bool AofPersistence::is_open() const noexcept {
    return file_.is_open();
}

bool AofPersistence::reopen_append_stream() {
    file_.close();
    file_.clear();
    file_.open(path_, std::ios::app | std::ios::binary);
    if (!file_.is_open()) {
        log::error("AOF: failed to reopen file after replay recovery: " + path_);
        return false;
    }
    return true;
}

bool AofPersistence::truncate_to(std::size_t bytes) {
    try {
        file_.flush();
        file_.close();
        std::filesystem::resize_file(path_, bytes);
        file_.clear();
        file_.open(path_, std::ios::app | std::ios::binary);
    } catch (const std::exception& ex) {
        log::error("AOF: failed to truncate file: " + std::string(ex.what()));
        return false;
    }

    if (!file_.is_open()) {
        log::error("AOF: failed to reopen file after truncate: " + path_);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Write path
// ---------------------------------------------------------------------------

bool AofPersistence::is_write_command(std::string_view name) noexcept {
    static const std::unordered_set<std::string_view> writes = {
        "SET",   "DEL",    "EXPIRE",  "PERSIST",
        "INCR",  "INCRBY", "DECR",    "DECRBY",
        "LPUSH", "RPUSH",  "LPOP",    "RPOP",
        "SADD",  "SREM",
        "HSET",  "HDEL",
    };
    return writes.count(name) > 0;
}

void AofPersistence::log(const Command& cmd) {
    // Reconstruct as a RESP array: [name, arg0, arg1, ...]
    RespArray arr;
    arr.elements.reserve(1 + cmd.args.size());
    arr.elements.push_back(RespBulkString{cmd.name});
    for (const auto& arg : cmd.args)
        arr.elements.push_back(RespBulkString{arg});

    const std::string wire = serialize(arr);

    std::lock_guard<std::mutex> lock(mutex_);
    file_.write(wire.data(), static_cast<std::streamsize>(wire.size()));
    file_.flush();  // flush to OS buffer after every write for safety
}

// ---------------------------------------------------------------------------
// Read / replay path
// ---------------------------------------------------------------------------

std::size_t AofPersistence::replay(const Dispatcher& dispatcher, KvStore& store) {
    std::ifstream in(path_, std::ios::binary);
    if (!in.is_open()) {
        log::info("AOF: no existing file at " + path_ + " — starting fresh");
        return 0;
    }

    // Slurp the whole file into memory.
    const std::string content(std::istreambuf_iterator<char>(in),
                               std::istreambuf_iterator<char>{});

    if (content.empty()) {
        log::info("AOF: file is empty — nothing to replay");
        return 0;
    }

    RespParser  parser;
    std::size_t count = 0;
    bool        first = true;
    bool        parse_error = false;

    while (true) {
        // Feed the whole buffer on first iteration, then drain with empty chunks.
        std::optional<RespValue> val;
        try {
            val = parser.feed(first ? std::string_view{content} : std::string_view{});
        } catch (const std::exception& ex) {
            log::error("AOF: parse error during replay — " + std::string(ex.what()));
            parse_error = true;
            break;
        }

        first = false;
        if (!val) break;

        auto cmd = parse_command(*val);
        if (!cmd) {
            log::warn("AOF: skipping non-command entry");
            continue;
        }

        // Suppress errors from individual commands during replay (e.g. a SET
        // that previously overwrote a key is fine to re-apply).
        try {
            dispatcher.dispatch(*cmd, store);
        } catch (const std::exception& ex) {
            log::warn("AOF: replay skipped '" + cmd->name + "': " + ex.what());
        }

        ++count;
    }

    const std::size_t trailing_bytes = parser.buffered_size();
    if (parse_error || trailing_bytes > 0) {
        const std::size_t valid_bytes = content.size() - trailing_bytes;
        log::warn("AOF: corruption/incomplete tail detected at byte "
                  + std::to_string(valid_bytes) + ", truncating file");
        truncate_to(valid_bytes);
    } else {
        reopen_append_stream();
    }

    log::info("AOF: replayed " + std::to_string(count) + " commands from " + path_);
    return count;
}

}  // namespace vortek
