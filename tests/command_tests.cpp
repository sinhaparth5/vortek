#include <catch2/catch_all.hpp>

#include "commands/command.hpp"
#include "commands/dispatcher.hpp"
#include "protocol/resp_serializer.hpp"

using namespace vortek;

// Helper: build a RESP array command and parse it back into a Command.
static Command make_cmd(std::vector<std::string> parts) {
    RespArray arr;
    for (auto& p : parts)
        arr.elements.push_back(RespBulkString{std::move(p)});
    RespValue val{std::move(arr)};
    auto cmd = parse_command(val);
    REQUIRE(cmd.has_value());
    return *cmd;
}

// Helper: dispatch and get string representation for comparison.
static RespValue run(Dispatcher& d, KvStore& s, std::vector<std::string> parts) {
    return d.dispatch(make_cmd(std::move(parts)), s);
}

// ---------------------------------------------------------------------------
// parse_command
// ---------------------------------------------------------------------------

TEST_CASE("parse_command: basic array", "[cmd]") {
    auto cmd = make_cmd({"set", "foo", "bar"});
    CHECK(cmd.name == "SET");
    REQUIRE(cmd.args.size() == 2);
    CHECK(cmd.args[0] == "foo");
    CHECK(cmd.args[1] == "bar");
}

TEST_CASE("parse_command: non-array returns nullopt", "[cmd]") {
    RespValue val{RespSimpleString{"PING"}};
    CHECK_FALSE(parse_command(val).has_value());
}

TEST_CASE("parse_command: empty array returns nullopt", "[cmd]") {
    RespValue val{RespArray{}};
    CHECK_FALSE(parse_command(val).has_value());
}

// ---------------------------------------------------------------------------
// PING
// ---------------------------------------------------------------------------

TEST_CASE("PING: no args", "[cmd][ping]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    auto r = run(d, s, {"PING"});
    REQUIRE(r.is<RespSimpleString>());
    CHECK(r.get<RespSimpleString>().value == "PONG");
}

TEST_CASE("PING: with message", "[cmd][ping]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    auto r = run(d, s, {"PING", "hello"});
    REQUIRE(r.is<RespBulkString>());
    CHECK(r.get<RespBulkString>().value == "hello");
}

// ---------------------------------------------------------------------------
// SET / GET
// ---------------------------------------------------------------------------

TEST_CASE("SET and GET basic", "[cmd][string]") {
    auto d = Dispatcher::make_default();
    KvStore s;

    auto r = run(d, s, {"SET", "k", "v"});
    REQUIRE(r.is<RespSimpleString>());
    CHECK(r.get<RespSimpleString>().value == "OK");

    auto g = run(d, s, {"GET", "k"});
    REQUIRE(g.is<RespBulkString>());
    CHECK(g.get<RespBulkString>().value == "v");
}

TEST_CASE("GET missing key returns null", "[cmd][string]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    auto r = run(d, s, {"GET", "missing"});
    CHECK(r.is<RespNull>());
}

TEST_CASE("SET NX: only sets when key absent", "[cmd][string]") {
    auto d = Dispatcher::make_default();
    KvStore s;

    CHECK(run(d, s, {"SET", "k", "first", "NX"}).is<RespSimpleString>());  // OK
    CHECK(run(d, s, {"SET", "k", "second", "NX"}).is<RespNull>());         // blocked
    CHECK(run(d, s, {"GET", "k"}).get<RespBulkString>().value == "first");
}

TEST_CASE("SET XX: only sets when key exists", "[cmd][string]") {
    auto d = Dispatcher::make_default();
    KvStore s;

    CHECK(run(d, s, {"SET", "k", "v", "XX"}).is<RespNull>());  // blocked, key absent
    run(d, s, {"SET", "k", "v"});
    CHECK(run(d, s, {"SET", "k", "new", "XX"}).is<RespSimpleString>());  // OK
    CHECK(run(d, s, {"GET", "k"}).get<RespBulkString>().value == "new");
}

TEST_CASE("SET EX: key expires", "[cmd][string]") {
    auto d = Dispatcher::make_default();
    KvStore s;

    run(d, s, {"SET", "k", "v", "EX", "1"});
    CHECK(run(d, s, {"GET", "k"}).is<RespBulkString>());

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    CHECK(run(d, s, {"GET", "k"}).is<RespNull>());
}

// ---------------------------------------------------------------------------
// DEL / EXISTS
// ---------------------------------------------------------------------------

TEST_CASE("DEL: removes key, returns count", "[cmd][generic]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    run(d, s, {"SET", "a", "1"});
    run(d, s, {"SET", "b", "2"});

    auto r = run(d, s, {"DEL", "a", "b", "c"});
    REQUIRE(r.is<RespInteger>());
    CHECK(r.get<RespInteger>().value == 2);
}

TEST_CASE("EXISTS: returns count", "[cmd][generic]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    run(d, s, {"SET", "k", "v"});

    CHECK(run(d, s, {"EXISTS", "k"}).get<RespInteger>().value == 1);
    CHECK(run(d, s, {"EXISTS", "nope"}).get<RespInteger>().value == 0);
}

// ---------------------------------------------------------------------------
// EXPIRE / TTL / PERSIST
// ---------------------------------------------------------------------------

TEST_CASE("EXPIRE and TTL", "[cmd][generic]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    run(d, s, {"SET", "k", "v"});

    CHECK(run(d, s, {"EXPIRE", "k", "10"}).get<RespInteger>().value == 1);
    auto ttl = run(d, s, {"TTL", "k"}).get<RespInteger>().value;
    CHECK(ttl > 0);
    CHECK(ttl <= 10);
}

TEST_CASE("TTL on key without expiry returns -1", "[cmd][generic]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    run(d, s, {"SET", "k", "v"});
    CHECK(run(d, s, {"TTL", "k"}).get<RespInteger>().value == -1);
}

TEST_CASE("TTL on missing key returns -2", "[cmd][generic]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    CHECK(run(d, s, {"TTL", "nope"}).get<RespInteger>().value == -2);
}

TEST_CASE("PERSIST removes expiry", "[cmd][generic]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    run(d, s, {"SET", "k", "v", "EX", "100"});
    CHECK(run(d, s, {"PERSIST", "k"}).get<RespInteger>().value == 1);
    CHECK(run(d, s, {"TTL", "k"}).get<RespInteger>().value == -1);
}

// ---------------------------------------------------------------------------
// INCR / DECR / INCRBY / DECRBY
// ---------------------------------------------------------------------------

TEST_CASE("INCR: starts at 0 if key absent", "[cmd][string]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    CHECK(run(d, s, {"INCR", "n"}).get<RespInteger>().value == 1);
    CHECK(run(d, s, {"INCR", "n"}).get<RespInteger>().value == 2);
}

TEST_CASE("DECR", "[cmd][string]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    run(d, s, {"SET", "n", "10"});
    CHECK(run(d, s, {"DECR", "n"}).get<RespInteger>().value == 9);
}

TEST_CASE("INCRBY / DECRBY", "[cmd][string]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    run(d, s, {"SET", "n", "100"});
    CHECK(run(d, s, {"INCRBY", "n", "50"}).get<RespInteger>().value == 150);
    CHECK(run(d, s, {"DECRBY", "n", "25"}).get<RespInteger>().value == 125);
}

TEST_CASE("INCR on non-integer value returns error", "[cmd][string]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    run(d, s, {"SET", "k", "notanumber"});
    CHECK(run(d, s, {"INCR", "k"}).is<RespError>());
}

TEST_CASE("unknown command returns error", "[cmd]") {
    auto d = Dispatcher::make_default();
    KvStore s;
    CHECK(run(d, s, {"FOOBAR"}).is<RespError>());
}
