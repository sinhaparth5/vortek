#include <catch2/catch_all.hpp>

#include "commands/command.hpp"
#include "commands/dispatcher.hpp"
#include "server/server_stats.hpp"

using namespace vortek;

namespace {
struct CompatCtx {
    ServerStats stats;
    Dispatcher  dispatcher{Dispatcher::make_default(stats)};
    KvStore     store;
};

RespValue run(Dispatcher& d, KvStore& s, std::vector<std::string> parts) {
    RespArray arr;
    for (auto& p : parts)
        arr.elements.push_back(RespBulkString{std::move(p)});
    auto cmd = parse_command(RespValue{std::move(arr)});
    REQUIRE(cmd.has_value());
    return d.dispatch(*cmd, s);
}
}  // namespace

TEST_CASE("Compatibility: command names are case-insensitive", "[compat]") {
    CompatCtx ctx;
    auto r = run(ctx.dispatcher, ctx.store, {"pInG"});
    REQUIRE(r.is<RespSimpleString>());
    CHECK(r.get<RespSimpleString>().value == "PONG");
}

TEST_CASE("Compatibility: unknown command error shape", "[compat]") {
    CompatCtx ctx;
    auto r = run(ctx.dispatcher, ctx.store, {"DOESNOTEXIST"});
    REQUIRE(r.is<RespError>());
    CHECK(r.get<RespError>().message == "ERR unknown command 'DOESNOTEXIST'");
}

TEST_CASE("Compatibility: wrong arity message shape", "[compat]") {
    CompatCtx ctx;
    auto r = run(ctx.dispatcher, ctx.store, {"GET"});
    REQUIRE(r.is<RespError>());
    CHECK(r.get<RespError>().message == "ERR wrong number of arguments for 'GET' command");
}

TEST_CASE("Compatibility: SET NX/XX conflict", "[compat]") {
    CompatCtx ctx;
    auto r = run(ctx.dispatcher, ctx.store, {"SET", "k", "v", "NX", "XX"});
    REQUIRE(r.is<RespError>());
    CHECK(r.get<RespError>().message.find("XX and NX options") != std::string::npos);
}

TEST_CASE("Compatibility: TTL sentinel values", "[compat]") {
    CompatCtx ctx;
    run(ctx.dispatcher, ctx.store, {"SET", "k", "v"});
    auto no_ttl = run(ctx.dispatcher, ctx.store, {"TTL", "k"});
    auto absent = run(ctx.dispatcher, ctx.store, {"TTL", "missing"});
    REQUIRE(no_ttl.is<RespInteger>());
    REQUIRE(absent.is<RespInteger>());
    CHECK(no_ttl.get<RespInteger>().value == -1);
    CHECK(absent.get<RespInteger>().value == -2);
}

TEST_CASE("Compatibility: WRONGTYPE error shape", "[compat]") {
    CompatCtx ctx;
    run(ctx.dispatcher, ctx.store, {"SET", "k", "v"});
    auto r = run(ctx.dispatcher, ctx.store, {"LPUSH", "k", "x"});
    REQUIRE(r.is<RespError>());
    CHECK(r.get<RespError>().message
          == "WRONGTYPE Operation against a key holding the wrong kind of value");
}

TEST_CASE("Compatibility: LRANGE inclusive semantics", "[compat]") {
    CompatCtx ctx;
    run(ctx.dispatcher, ctx.store, {"RPUSH", "l", "a", "b", "c", "d"});
    auto r = run(ctx.dispatcher, ctx.store, {"LRANGE", "l", "1", "2"});
    REQUIRE(r.is<RespArray>());
    const auto& elems = r.get<RespArray>().elements;
    REQUIRE(elems.size() == 2);
    CHECK(elems[0].get<RespBulkString>().value == "b");
    CHECK(elems[1].get<RespBulkString>().value == "c");
}
