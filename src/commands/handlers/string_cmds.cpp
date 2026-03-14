#include "string_cmds.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>

namespace vortek::handlers {

namespace {

RespValue err(std::string msg) {
    return RespError{std::move(msg)};
}

RespValue wrong_args(std::string_view name) {
    return RespError{"ERR wrong number of arguments for '" + std::string(name) + "' command"};
}

// ---------------------------------------------------------------------------

// SET key value [EX seconds] [PX milliseconds] [NX] [XX]
// → +OK, or $-1 (null) when NX/XX condition not met
RespValue cmd_set(const Command& cmd, KvStore& store) {
    if (cmd.args.size() < 2)
        return wrong_args(cmd.name);

    const auto& key   = cmd.args[0];
    const auto& value = cmd.args[1];

    std::chrono::milliseconds ttl{0};
    bool nx = false;  // only set if NOT exists
    bool xx = false;  // only set if EXISTS

    // Parse optional flags (case-insensitive)
    for (std::size_t i = 2; i < cmd.args.size(); ++i) {
        std::string flag = cmd.args[i];
        std::transform(flag.begin(), flag.end(), flag.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

        if (flag == "NX") {
            nx = true;
        } else if (flag == "XX") {
            xx = true;
        } else if (flag == "EX") {
            if (i + 1 >= cmd.args.size())
                return err("ERR syntax error");
            int64_t secs{};
            const auto& s = cmd.args[++i];
            auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), secs);
            if (ec != std::errc{} || ptr != s.data() + s.size() || secs <= 0)
                return err("ERR invalid expire time in 'set' command");
            ttl = std::chrono::seconds{secs};
        } else if (flag == "PX") {
            if (i + 1 >= cmd.args.size())
                return err("ERR syntax error");
            int64_t ms{};
            const auto& s = cmd.args[++i];
            auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), ms);
            if (ec != std::errc{} || ptr != s.data() + s.size() || ms <= 0)
                return err("ERR invalid expire time in 'set' command");
            ttl = std::chrono::milliseconds{ms};
        } else {
            return err("ERR syntax error");
        }
    }

    if (nx && xx)
        return err("ERR XX and NX options at the same time are not compatible");

    bool exists = store.exists(key);
    if (nx && exists)  return RespNull{};
    if (xx && !exists) return RespNull{};

    store.set(key, Value{value}, ttl);
    return RespSimpleString{"OK"};
}

// GET key
// → bulk string or null
RespValue cmd_get(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1)
        return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespNull{};
    return RespBulkString{val->as_string()};
}

// Shared logic for INCR / DECR / INCRBY / DECRBY.
// delta is positive for INCR/INCRBY, negative for DECR/DECRBY.
RespValue apply_delta(const std::string& key, int64_t delta, KvStore& store) {
    auto current = store.get(key);

    int64_t num = 0;
    if (current) {
        const auto& s = current->as_string();
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), num);
        if (ec != std::errc{} || ptr != s.data() + s.size())
            return RespError{"ERR value is not an integer or out of range"};
    }

    // Overflow check
    if ((delta > 0 && num > std::numeric_limits<int64_t>::max() - delta) ||
        (delta < 0 && num < std::numeric_limits<int64_t>::min() - delta))
        return RespError{"ERR increment or decrement would overflow"};

    num += delta;
    store.set(key, Value{std::to_string(num)});
    return RespInteger{num};
}

// INCR key → integer
RespValue cmd_incr(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1) return wrong_args(cmd.name);
    return apply_delta(cmd.args[0], 1, store);
}

// DECR key → integer
RespValue cmd_decr(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1) return wrong_args(cmd.name);
    return apply_delta(cmd.args[0], -1, store);
}

// INCRBY key amount → integer
RespValue cmd_incrby(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 2) return wrong_args(cmd.name);

    int64_t delta{};
    const auto& s = cmd.args[1];
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), delta);
    if (ec != std::errc{} || ptr != s.data() + s.size())
        return err("ERR value is not an integer or out of range");

    return apply_delta(cmd.args[0], delta, store);
}

// DECRBY key amount → integer
RespValue cmd_decrby(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 2) return wrong_args(cmd.name);

    int64_t delta{};
    const auto& s = cmd.args[1];
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), delta);
    if (ec != std::errc{} || ptr != s.data() + s.size())
        return err("ERR value is not an integer or out of range");

    // Negate: DECRBY 5 means delta = -5
    if (delta == std::numeric_limits<int64_t>::min())
        return err("ERR increment or decrement would overflow");

    return apply_delta(cmd.args[0], -delta, store);
}

}  // namespace

// ---------------------------------------------------------------------------

void register_string(Dispatcher& dispatcher) {
    dispatcher.register_handler("SET",    cmd_set);
    dispatcher.register_handler("GET",    cmd_get);
    dispatcher.register_handler("INCR",   cmd_incr);
    dispatcher.register_handler("DECR",   cmd_decr);
    dispatcher.register_handler("INCRBY", cmd_incrby);
    dispatcher.register_handler("DECRBY", cmd_decrby);
}

}  // namespace vortek::handlers
