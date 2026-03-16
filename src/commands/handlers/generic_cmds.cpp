#include "generic_cmds.hpp"

#include <algorithm>
#include <atomic>
#include <charconv>
#include <chrono>
#include <string>

namespace vortek::handlers {

namespace {

RespValue err(std::string msg) {
    return RespError{std::move(msg)};
}

RespValue wrong_args(std::string_view name) {
    return RespError{"ERR wrong number of arguments for '" + std::string(name) + "' command"};
}

// ---------------------------------------------------------------------------

// PING [message]
RespValue cmd_ping(const Command& cmd, KvStore& /*store*/) {
    if (cmd.args.empty())
        return RespSimpleString{"PONG"};
    if (cmd.args.size() == 1)
        return RespBulkString{cmd.args[0]};
    return wrong_args(cmd.name);
}

// DEL key [key ...]
RespValue cmd_del(const Command& cmd, KvStore& store) {
    if (cmd.args.empty())
        return wrong_args(cmd.name);
    int64_t count = 0;
    for (const auto& key : cmd.args)
        count += store.del(key);
    return RespInteger{count};
}

// EXISTS key [key ...]
RespValue cmd_exists(const Command& cmd, KvStore& store) {
    if (cmd.args.empty())
        return wrong_args(cmd.name);
    int64_t count = 0;
    for (const auto& key : cmd.args)
        if (store.exists(key)) ++count;
    return RespInteger{count};
}

// EXPIRE key seconds
RespValue cmd_expire(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 2)
        return wrong_args(cmd.name);
    int64_t secs{};
    const auto& s = cmd.args[1];
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), secs);
    if (ec != std::errc{} || ptr != s.data() + s.size() || secs < 0)
        return err("ERR value is not an integer or out of range");
    return RespInteger{store.expire(cmd.args[0], std::chrono::seconds{secs}) ? 1 : 0};
}

// TTL key
RespValue cmd_ttl(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1)
        return wrong_args(cmd.name);
    int64_t ms = store.ttl(cmd.args[0]);
    if (ms < 0)
        return RespInteger{ms};
    return RespInteger{(ms + 999) / 1000};
}

// PERSIST key
RespValue cmd_persist(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1)
        return wrong_args(cmd.name);
    return RespInteger{store.persist(cmd.args[0]) ? 1 : 0};
}

// HEALTH
RespValue cmd_health(const Command& cmd, KvStore& /*store*/) {
    if (!cmd.args.empty())
        return wrong_args(cmd.name);
    return RespSimpleString{"OK"};
}

// READY
RespValue cmd_ready(const Command& cmd, KvStore& /*store*/, const ServerStats& stats) {
    if (!cmd.args.empty())
        return wrong_args(cmd.name);
    if (!stats.ready.load(std::memory_order_relaxed))
        return RespError{"ERR server not ready"};
    return RespSimpleString{"READY"};
}

// METRICS (Prometheus exposition text)
RespValue cmd_metrics(const Command& cmd, KvStore& store, const ServerStats& stats) {
    if (!cmd.args.empty())
        return wrong_args(cmd.name);

    const int64_t uptime = std::max<int64_t>(1, stats.uptime_seconds());
    const double  qps = static_cast<double>(stats.total_commands.load()) / static_cast<double>(uptime);

    std::string body;
    body += "vortek_uptime_seconds " + std::to_string(stats.uptime_seconds()) + "\n";
    body += "vortek_connected_clients " + std::to_string(stats.connected_clients.load()) + "\n";
    body += "vortek_total_commands_processed " + std::to_string(stats.total_commands.load()) + "\n";
    body += "vortek_commands_per_second " + std::to_string(qps) + "\n";
    body += "vortek_key_count " + std::to_string(store.size()) + "\n";
    body += "vortek_memory_bytes_approx " + std::to_string(store.approx_memory_bytes()) + "\n";
    body += "vortek_ready " + std::to_string(stats.ready.load(std::memory_order_relaxed) ? 1 : 0) + "\n";
    return RespBulkString{std::move(body)};
}

// ---------------------------------------------------------------------------
// INFO [section]
//
// Sections: server | clients | stats | keyspace | all (default)
// ---------------------------------------------------------------------------

std::string section_server(const ServerStats& stats) {
    int64_t uptime_sec  = stats.uptime_seconds();
    int64_t uptime_days = uptime_sec / 86400;
    return std::string("# Server\r\n")
         + "vortek_version:0.1.0\r\n"
         + "tcp_port:" + std::to_string(stats.port) + "\r\n"
         + "uptime_in_seconds:" + std::to_string(uptime_sec) + "\r\n"
         + "uptime_in_days:" + std::to_string(uptime_days) + "\r\n"
         + "aof_enabled:" + (stats.aof_enabled ? "1" : "0") + "\r\n";
}

std::string section_clients(const ServerStats& stats) {
    return std::string("# Clients\r\n")
         + "connected_clients:" + std::to_string(stats.connected_clients.load()) + "\r\n";
}

std::string section_stats(const ServerStats& stats) {
    return std::string("# Stats\r\n")
         + "total_commands_processed:" + std::to_string(stats.total_commands.load()) + "\r\n";
}

std::string section_keyspace(const KvStore& store) {
    std::size_t keys = store.size();
    if (keys == 0)
        return "# Keyspace\r\n";
    return std::string("# Keyspace\r\n")
         + "db0:keys=" + std::to_string(keys) + "\r\n";
}

RespValue cmd_info(const Command& cmd, KvStore& store, const ServerStats& stats) {
    if (cmd.args.size() > 1)
        return RespError{"ERR syntax error"};

    // Determine which section(s) to return.
    std::string section = cmd.args.empty() ? "all" : cmd.args[0];
    // Normalise to lowercase.
    for (auto& c : section)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::string body;

    if (section == "server" || section == "all" || section == "everything") {
        body += section_server(stats);
    }
    if (section == "clients" || section == "all" || section == "everything") {
        if (!body.empty()) body += "\r\n";
        body += section_clients(stats);
    }
    if (section == "stats" || section == "all" || section == "everything") {
        if (!body.empty()) body += "\r\n";
        body += section_stats(stats);
    }
    if (section == "keyspace" || section == "all" || section == "everything") {
        if (!body.empty()) body += "\r\n";
        body += section_keyspace(store);
    }

    if (body.empty())
        return RespError{"ERR unknown INFO section '" + std::string(cmd.args[0]) + "'"};

    return RespBulkString{std::move(body)};
}

}  // namespace

// ---------------------------------------------------------------------------

void register_generic(Dispatcher& dispatcher, ServerStats& stats) {
    dispatcher.register_handler("PING",    cmd_ping);
    dispatcher.register_handler("DEL",     cmd_del);
    dispatcher.register_handler("EXISTS",  cmd_exists);
    dispatcher.register_handler("EXPIRE",  cmd_expire);
    dispatcher.register_handler("TTL",     cmd_ttl);
    dispatcher.register_handler("PERSIST", cmd_persist);
    dispatcher.register_handler("HEALTH",  cmd_health);

    // Capture stats by reference — safe because stats outlives the dispatcher.
    dispatcher.register_handler("INFO",
        [&stats](const Command& cmd, KvStore& store) {
            return cmd_info(cmd, store, stats);
        });
    dispatcher.register_handler("READY",
        [&stats](const Command& cmd, KvStore& store) {
            return cmd_ready(cmd, store, stats);
        });
    dispatcher.register_handler("METRICS",
        [&stats](const Command& cmd, KvStore& store) {
            return cmd_metrics(cmd, store, stats);
        });
}

}  // namespace vortek::handlers
