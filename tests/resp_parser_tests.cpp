#include <catch2/catch_all.hpp>

#include "protocol/resp_parser.hpp"
#include "protocol/resp_serializer.hpp"

using namespace vortek;

// ---------------------------------------------------------------------------
// Parser tests
// ---------------------------------------------------------------------------

TEST_CASE("RespParser: simple string", "[resp][parser]") {
    RespParser p;
    auto val = p.feed("+OK\r\n");
    REQUIRE(val.has_value());
    REQUIRE(val->is<RespSimpleString>());
    CHECK(val->get<RespSimpleString>().value == "OK");
}

TEST_CASE("RespParser: error", "[resp][parser]") {
    RespParser p;
    auto val = p.feed("-ERR unknown command\r\n");
    REQUIRE(val.has_value());
    REQUIRE(val->is<RespError>());
    CHECK(val->get<RespError>().message == "ERR unknown command");
}

TEST_CASE("RespParser: integer", "[resp][parser]") {
    RespParser p;
    auto val = p.feed(":1000\r\n");
    REQUIRE(val.has_value());
    REQUIRE(val->is<RespInteger>());
    CHECK(val->get<RespInteger>().value == 1000);
}

TEST_CASE("RespParser: negative integer", "[resp][parser]") {
    RespParser p;
    auto val = p.feed(":-42\r\n");
    REQUIRE(val.has_value());
    CHECK(val->get<RespInteger>().value == -42);
}

TEST_CASE("RespParser: bulk string", "[resp][parser]") {
    RespParser p;
    auto val = p.feed("$6\r\nfoobar\r\n");
    REQUIRE(val.has_value());
    REQUIRE(val->is<RespBulkString>());
    CHECK(val->get<RespBulkString>().value == "foobar");
}

TEST_CASE("RespParser: null bulk string", "[resp][parser]") {
    RespParser p;
    auto val = p.feed("$-1\r\n");
    REQUIRE(val.has_value());
    CHECK(val->is<RespNull>());
}

TEST_CASE("RespParser: array", "[resp][parser]") {
    RespParser p;
    auto val = p.feed("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
    REQUIRE(val.has_value());
    REQUIRE(val->is<RespArray>());
    const auto& arr = val->get<RespArray>();
    REQUIRE(arr.elements.size() == 2);
    CHECK(arr.elements[0].get<RespBulkString>().value == "foo");
    CHECK(arr.elements[1].get<RespBulkString>().value == "bar");
}

TEST_CASE("RespParser: null array", "[resp][parser]") {
    RespParser p;
    auto val = p.feed("*-1\r\n");
    REQUIRE(val.has_value());
    CHECK(val->is<RespNull>());
}

TEST_CASE("RespParser: empty array", "[resp][parser]") {
    RespParser p;
    auto val = p.feed("*0\r\n");
    REQUIRE(val.has_value());
    REQUIRE(val->is<RespArray>());
    CHECK(val->get<RespArray>().elements.empty());
}

TEST_CASE("RespParser: incremental feed", "[resp][parser]") {
    RespParser p;

    // Feed one byte at a time
    std::string_view msg = "+PONG\r\n";
    std::optional<RespValue> result;
    for (std::size_t i = 0; i < msg.size(); ++i) {
        result = p.feed(msg.substr(i, 1));
        if (i < msg.size() - 1)
            CHECK_FALSE(result.has_value());
    }
    REQUIRE(result.has_value());
    CHECK(result->get<RespSimpleString>().value == "PONG");
}

TEST_CASE("RespParser: typical PING command array", "[resp][parser]") {
    RespParser p;
    // redis-cli sends: *1\r\n$4\r\nPING\r\n
    auto val = p.feed("*1\r\n$4\r\nPING\r\n");
    REQUIRE(val.has_value());
    REQUIRE(val->is<RespArray>());
    const auto& arr = val->get<RespArray>();
    REQUIRE(arr.elements.size() == 1);
    CHECK(arr.elements[0].get<RespBulkString>().value == "PING");
}

TEST_CASE("RespParser: SET command array", "[resp][parser]") {
    RespParser p;
    // *3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nhello\r\n
    auto val = p.feed("*3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nhello\r\n");
    REQUIRE(val.has_value());
    REQUIRE(val->is<RespArray>());
    const auto& arr = val->get<RespArray>();
    REQUIRE(arr.elements.size() == 3);
    CHECK(arr.elements[0].get<RespBulkString>().value == "SET");
    CHECK(arr.elements[1].get<RespBulkString>().value == "key");
    CHECK(arr.elements[2].get<RespBulkString>().value == "hello");
}

TEST_CASE("RespParser: invalid type byte throws", "[resp][parser]") {
    RespParser p;
    CHECK_THROWS_AS(p.feed("!bad\r\n"), std::runtime_error);
}

// ---------------------------------------------------------------------------
// Serializer tests
// ---------------------------------------------------------------------------

TEST_CASE("serialize: null", "[resp][serializer]") {
    CHECK(serialize(RespNull{}) == "$-1\r\n");
}

TEST_CASE("serialize: simple string", "[resp][serializer]") {
    CHECK(serialize(RespSimpleString{"OK"}) == "+OK\r\n");
}

TEST_CASE("serialize: error", "[resp][serializer]") {
    CHECK(serialize(RespError{"ERR oops"}) == "-ERR oops\r\n");
}

TEST_CASE("serialize: integer", "[resp][serializer]") {
    CHECK(serialize(RespInteger{42}) == ":42\r\n");
    CHECK(serialize(RespInteger{-1}) == ":-1\r\n");
}

TEST_CASE("serialize: bulk string", "[resp][serializer]") {
    CHECK(serialize(RespBulkString{"hello"}) == "$5\r\nhello\r\n");
    CHECK(serialize(RespBulkString{""}) == "$0\r\n\r\n");
}

TEST_CASE("serialize: array", "[resp][serializer]") {
    RespArray arr;
    arr.elements.push_back(RespBulkString{"foo"});
    arr.elements.push_back(RespInteger{99});
    CHECK(serialize(arr) == "*2\r\n$3\r\nfoo\r\n:99\r\n");
}

TEST_CASE("serialize: roundtrip bulk string", "[resp][serializer]") {
    RespBulkString orig{"hello world"};
    std::string wire = serialize(orig);

    RespParser p;
    auto parsed = p.feed(wire);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->is<RespBulkString>());
    CHECK(parsed->get<RespBulkString>().value == orig.value);
}

TEST_CASE("serialize: roundtrip array", "[resp][serializer]") {
    RespArray orig;
    orig.elements.push_back(RespBulkString{"SET"});
    orig.elements.push_back(RespBulkString{"mykey"});
    orig.elements.push_back(RespBulkString{"myval"});
    std::string wire = serialize(orig);

    RespParser p;
    auto parsed = p.feed(wire);
    REQUIRE(parsed.has_value());
    REQUIRE(parsed->is<RespArray>());
    const auto& arr = parsed->get<RespArray>();
    REQUIRE(arr.elements.size() == 3);
    CHECK(arr.elements[0].get<RespBulkString>().value == "SET");
    CHECK(arr.elements[1].get<RespBulkString>().value == "mykey");
    CHECK(arr.elements[2].get<RespBulkString>().value == "myval");
}
