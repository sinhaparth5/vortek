#include "generic_cmds.hpp"

#include <charconv>
#include <chrono>

namespace vortek::handlers {

namespace {

// Helper: build an error reply.
RespValue err(std::string msg) {
    return RespError{std::move(msg)};
}

RespValue wrong_args(std::string_view name) {
    return RespError{"ERR wrong number of arguments for '" + std::string(name) + "' command"};
}

// ---------------------------------------------------------------------------

// PING [message]
// → +PONG  or  $<n>\r\n<message>\r\n
RespValue cmd_ping(const Command& cmd, KvStore& /*store*/) {
    if (cmd.args.empty())
        return RespSimpleString{"PONG"};
    if (cmd.args.size() == 1)
        return RespBulkString{cmd.args[0]};
    return wrong_args(cmd.name);
}

// DEL key [key ...]
// → integer: number of keys actually removed
RespValue cmd_del(const Command& cmd, KvStore& store) {
    if (cmd.args.empty())
        return wrong_args(cmd.name);

    int64_t count = 0;
    for (const auto& key : cmd.args)
        count += store.del(key);

    return RespInteger{count};
}

// EXISTS key [key ...]
// → integer: number of supplied keys that exist (duplicates counted separately)
RespValue cmd_exists(const Command& cmd, KvStore& store) {
    if (cmd.args.empty())
        return wrong_args(cmd.name);

    int64_t count = 0;
    for (const auto& key : cmd.args)
        if (store.exists(key)) ++count;

    return RespInteger{count};
}

// EXPIRE key seconds
// → 1 if the timeout was set, 0 if the key doesn't exist
RespValue cmd_expire(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 2)
        return wrong_args(cmd.name);

    int64_t secs{};
    const auto& s = cmd.args[1];
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), secs);
    if (ec != std::errc{} || ptr != s.data() + s.size() || secs < 0)
        return err("ERR value is not an integer or out of range");

    bool ok = store.expire(cmd.args[0], std::chrono::seconds{secs});
    return RespInteger{ok ? 1 : 0};
}

// TTL key
// → remaining seconds, -1 (no expiry), -2 (not found)
RespValue cmd_ttl(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1)
        return wrong_args(cmd.name);

    int64_t ms = store.ttl(cmd.args[0]);
    if (ms < 0)
        return RespInteger{ms};  // -1 or -2 pass through as-is

    // Convert milliseconds → seconds, rounding up to match Redis behaviour.
    return RespInteger{(ms + 999) / 1000};
}

// PERSIST key
// → 1 if timeout removed, 0 if key has no timeout or doesn't exist
RespValue cmd_persist(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1)
        return wrong_args(cmd.name);

    bool ok = store.persist(cmd.args[0]);
    return RespInteger{ok ? 1 : 0};
}

}  // namespace

// ---------------------------------------------------------------------------

void register_generic(Dispatcher& dispatcher) {
    dispatcher.register_handler("PING",    cmd_ping);
    dispatcher.register_handler("DEL",     cmd_del);
    dispatcher.register_handler("EXISTS",  cmd_exists);
    dispatcher.register_handler("EXPIRE",  cmd_expire);
    dispatcher.register_handler("TTL",     cmd_ttl);
    dispatcher.register_handler("PERSIST", cmd_persist);
}

}  // namespace vortek::handlers
