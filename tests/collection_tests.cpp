#include <catch2/catch_all.hpp>

#include "commands/command.hpp"
#include "commands/dispatcher.hpp"
#include "server/server_stats.hpp"

using namespace vortek;

struct Ctx {
    ServerStats stats;
    Dispatcher  dispatcher{Dispatcher::make_default(stats)};
    KvStore     store;
};

static RespValue run(Dispatcher& d, KvStore& s, std::vector<std::string> parts) {
    RespArray arr;
    for (auto& p : parts)
        arr.elements.push_back(RespBulkString{std::move(p)});
    auto cmd = parse_command(RespValue{std::move(arr)});
    REQUIRE(cmd.has_value());
    return d.dispatch(*cmd, s);
}

// ===========================================================================
// LIST
// ===========================================================================

TEST_CASE("RPUSH / LLEN / LRANGE basics", "[list]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    CHECK(run(d, s, {"RPUSH", "l", "a", "b", "c"}).get<RespInteger>().value == 3);
    CHECK(run(d, s, {"LLEN", "l"}).get<RespInteger>().value == 3);

    auto r = run(d, s, {"LRANGE", "l", "0", "-1"});
    REQUIRE(r.is<RespArray>());
    const auto& elems = r.get<RespArray>().elements;
    REQUIRE(elems.size() == 3);
    CHECK(elems[0].get<RespBulkString>().value == "a");
    CHECK(elems[1].get<RespBulkString>().value == "b");
    CHECK(elems[2].get<RespBulkString>().value == "c");
}

TEST_CASE("LPUSH prepends in order", "[list]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    // LPUSH a b c → list is [c, b, a]
    run(d, s, {"LPUSH", "l", "a", "b", "c"});
    auto r = run(d, s, {"LRANGE", "l", "0", "-1"});
    const auto& elems = r.get<RespArray>().elements;
    REQUIRE(elems.size() == 3);
    CHECK(elems[0].get<RespBulkString>().value == "c");
    CHECK(elems[2].get<RespBulkString>().value == "a");
}

TEST_CASE("LPOP / RPOP", "[list]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"RPUSH", "l", "x", "y", "z"});
    CHECK(run(d, s, {"LPOP", "l"}).get<RespBulkString>().value == "x");
    CHECK(run(d, s, {"RPOP", "l"}).get<RespBulkString>().value == "z");
    CHECK(run(d, s, {"LLEN", "l"}).get<RespInteger>().value == 1);
}

TEST_CASE("LPOP on empty / missing key returns null", "[list]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    CHECK(run(d, s, {"LPOP", "missing"}).is<RespNull>());
    run(d, s, {"RPUSH", "l", "only"});
    run(d, s, {"LPOP", "l"});
    CHECK(run(d, s, {"LPOP", "l"}).is<RespNull>());
}

TEST_CASE("LINDEX", "[list]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"RPUSH", "l", "a", "b", "c"});
    CHECK(run(d, s, {"LINDEX", "l", "0"}).get<RespBulkString>().value == "a");
    CHECK(run(d, s, {"LINDEX", "l", "-1"}).get<RespBulkString>().value == "c");
    CHECK(run(d, s, {"LINDEX", "l", "99"}).is<RespNull>());
}

TEST_CASE("LRANGE with negative / out-of-bounds indices", "[list]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"RPUSH", "l", "a", "b", "c", "d"});

    auto r1 = run(d, s, {"LRANGE", "l", "-2", "-1"});
    const auto& e1 = r1.get<RespArray>().elements;
    REQUIRE(e1.size() == 2);
    CHECK(e1[0].get<RespBulkString>().value == "c");
    CHECK(e1[1].get<RespBulkString>().value == "d");

    // start > stop → empty array
    CHECK(run(d, s, {"LRANGE", "l", "3", "1"}).get<RespArray>().elements.empty());
}

TEST_CASE("List WRONGTYPE error", "[list]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"SET", "k", "str"});
    CHECK(run(d, s, {"LPUSH", "k", "x"}).is<RespError>());
    CHECK(run(d, s, {"LLEN",  "k"}).is<RespError>());
}

// ===========================================================================
// SET
// ===========================================================================

TEST_CASE("SADD / SCARD / SMEMBERS", "[set]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    CHECK(run(d, s, {"SADD", "s", "a", "b", "c"}).get<RespInteger>().value == 3);
    CHECK(run(d, s, {"SADD", "s", "b", "d"}).get<RespInteger>().value == 1);  // b already in
    CHECK(run(d, s, {"SCARD", "s"}).get<RespInteger>().value == 4);

    auto r = run(d, s, {"SMEMBERS", "s"});
    REQUIRE(r.is<RespArray>());
    CHECK(r.get<RespArray>().elements.size() == 4);
}

TEST_CASE("SISMEMBER", "[set]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"SADD", "s", "hello"});
    CHECK(run(d, s, {"SISMEMBER", "s", "hello"}).get<RespInteger>().value == 1);
    CHECK(run(d, s, {"SISMEMBER", "s", "world"}).get<RespInteger>().value == 0);
    CHECK(run(d, s, {"SISMEMBER", "missing", "x"}).get<RespInteger>().value == 0);
}

TEST_CASE("SREM removes members", "[set]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"SADD", "s", "a", "b", "c"});
    CHECK(run(d, s, {"SREM", "s", "a", "z"}).get<RespInteger>().value == 1);
    CHECK(run(d, s, {"SCARD", "s"}).get<RespInteger>().value == 2);
}

TEST_CASE("SREM on last member removes key", "[set]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"SADD", "s", "only"});
    run(d, s, {"SREM", "s", "only"});
    CHECK(run(d, s, {"SCARD", "s"}).get<RespInteger>().value == 0);
}

TEST_CASE("Set WRONGTYPE error", "[set]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"SET", "k", "str"});
    CHECK(run(d, s, {"SADD", "k", "x"}).is<RespError>());
    CHECK(run(d, s, {"SCARD", "k"}).is<RespError>());
}

// ===========================================================================
// HASH
// ===========================================================================

TEST_CASE("HSET / HGET / HLEN basics", "[hash]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    CHECK(run(d, s, {"HSET", "h", "f1", "v1", "f2", "v2"}).get<RespInteger>().value == 2);
    CHECK(run(d, s, {"HGET", "h", "f1"}).get<RespBulkString>().value == "v1");
    CHECK(run(d, s, {"HGET", "h", "f2"}).get<RespBulkString>().value == "v2");
    CHECK(run(d, s, {"HGET", "h", "missing"}).is<RespNull>());
    CHECK(run(d, s, {"HLEN", "h"}).get<RespInteger>().value == 2);
}

TEST_CASE("HSET update does not count as added", "[hash]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"HSET", "h", "f", "old"});
    CHECK(run(d, s, {"HSET", "h", "f", "new"}).get<RespInteger>().value == 0);
    CHECK(run(d, s, {"HGET", "h", "f"}).get<RespBulkString>().value == "new");
}

TEST_CASE("HDEL removes fields", "[hash]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"HSET", "h", "a", "1", "b", "2", "c", "3"});
    CHECK(run(d, s, {"HDEL", "h", "a", "z"}).get<RespInteger>().value == 1);
    CHECK(run(d, s, {"HLEN", "h"}).get<RespInteger>().value == 2);
}

TEST_CASE("HEXISTS", "[hash]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"HSET", "h", "field", "val"});
    CHECK(run(d, s, {"HEXISTS", "h", "field"}).get<RespInteger>().value == 1);
    CHECK(run(d, s, {"HEXISTS", "h", "nope"}).get<RespInteger>().value == 0);
    CHECK(run(d, s, {"HEXISTS", "missing", "f"}).get<RespInteger>().value == 0);
}

TEST_CASE("HGETALL returns interleaved field/value pairs", "[hash]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"HSET", "h", "k1", "v1"});
    auto r = run(d, s, {"HGETALL", "h"});
    REQUIRE(r.is<RespArray>());
    CHECK(r.get<RespArray>().elements.size() == 2);
}

TEST_CASE("HGETALL on missing key returns empty array", "[hash]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;
    CHECK(run(d, s, {"HGETALL", "missing"}).get<RespArray>().elements.empty());
}

TEST_CASE("Hash WRONGTYPE error", "[hash]") {
    Ctx ctx;
    auto& d = ctx.dispatcher; auto& s = ctx.store;

    run(d, s, {"SET", "k", "str"});
    CHECK(run(d, s, {"HSET", "k", "f", "v"}).is<RespError>());
    CHECK(run(d, s, {"HGET", "k", "f"}).is<RespError>());
}
