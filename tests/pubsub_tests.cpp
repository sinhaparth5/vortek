#include <catch2/catch_all.hpp>

#include "commands/command.hpp"
#include "commands/dispatcher.hpp"
#include "server/pubsub_broker.hpp"
#include "server/server_stats.hpp"

using namespace vortek;

// ---------------------------------------------------------------------------
// PubSubBroker unit tests
// ---------------------------------------------------------------------------

TEST_CASE("subscribe and publish delivers message", "[pubsub]") {
    PubSubBroker broker;
    std::string  received;

    broker.subscribe("news", 1, [&](std::string msg) { received = std::move(msg); });
    const int64_t count = broker.publish("news", "hello");

    CHECK(count == 1);
    CHECK(received == "hello");
}

TEST_CASE("publish to channel with no subscribers returns 0", "[pubsub]") {
    PubSubBroker broker;
    CHECK(broker.publish("empty", "msg") == 0);
}

TEST_CASE("multiple subscribers all receive message", "[pubsub]") {
    PubSubBroker broker;
    int          calls = 0;

    broker.subscribe("ch", 1, [&](std::string) { ++calls; });
    broker.subscribe("ch", 2, [&](std::string) { ++calls; });
    broker.subscribe("ch", 3, [&](std::string) { ++calls; });

    CHECK(broker.publish("ch", "x") == 3);
    CHECK(calls == 3);
}

TEST_CASE("unsubscribe stops delivery", "[pubsub]") {
    PubSubBroker broker;
    int          calls = 0;

    broker.subscribe("ch", 1, [&](std::string) { ++calls; });
    broker.unsubscribe("ch", 1);

    CHECK(broker.publish("ch", "x") == 0);
    CHECK(calls == 0);
}

TEST_CASE("unsubscribe_all removes all channels for subscriber", "[pubsub]") {
    PubSubBroker broker;
    int          calls = 0;

    broker.subscribe("a", 1, [&](std::string) { ++calls; });
    broker.subscribe("b", 1, [&](std::string) { ++calls; });
    broker.subscribe("c", 2, [&](std::string) { ++calls; });  // different id

    const auto removed = broker.unsubscribe_all(1);
    CHECK(removed.size() == 2);

    broker.publish("a", "x");
    broker.publish("b", "x");
    broker.publish("c", "x");  // subscriber 2 still here

    CHECK(calls == 1);  // only subscriber 2 on "c" fired
}

TEST_CASE("re-subscribe replaces write callback", "[pubsub]") {
    PubSubBroker broker;
    std::string  last;

    broker.subscribe("ch", 1, [&](std::string) { last = "first"; });
    broker.subscribe("ch", 1, [&](std::string) { last = "second"; });  // replace

    CHECK(broker.publish("ch", "x") == 1);
    CHECK(last == "second");
}

// ---------------------------------------------------------------------------
// PUBLISH command through dispatcher
// ---------------------------------------------------------------------------

TEST_CASE("PUBLISH via dispatcher returns subscriber count", "[pubsub]") {
    ServerStats  stats;
    PubSubBroker broker;
    Dispatcher   d = Dispatcher::make_default(stats, &broker);
    KvStore      store;

    int received = 0;
    broker.subscribe("events", 42, [&](std::string) { ++received; });

    RespArray arr;
    arr.elements.push_back(RespBulkString{"PUBLISH"});
    arr.elements.push_back(RespBulkString{"events"});
    arr.elements.push_back(RespBulkString{"payload"});
    auto cmd = parse_command(RespValue{std::move(arr)});
    REQUIRE(cmd.has_value());

    auto r = d.dispatch(*cmd, store);
    REQUIRE(r.is<RespInteger>());
    CHECK(r.get<RespInteger>().value == 1);
    CHECK(received == 1);
}

TEST_CASE("PUBLISH with no broker registered returns error", "[pubsub]") {
    ServerStats stats;
    Dispatcher  d = Dispatcher::make_default(stats);  // no broker
    KvStore     store;

    RespArray arr;
    arr.elements.push_back(RespBulkString{"PUBLISH"});
    arr.elements.push_back(RespBulkString{"ch"});
    arr.elements.push_back(RespBulkString{"msg"});
    auto cmd = parse_command(RespValue{std::move(arr)});
    REQUIRE(cmd.has_value());

    // Without broker, PUBLISH is not registered → unknown command error
    auto r = d.dispatch(*cmd, store);
    CHECK(r.is<RespError>());
}
