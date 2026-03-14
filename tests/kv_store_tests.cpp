#include <catch2/catch_all.hpp>

#include "core/kv_store.hpp"

#include <thread>
#include <vector>

using namespace vortek;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// Basic set / get
// ---------------------------------------------------------------------------

TEST_CASE("KvStore: set and get", "[kv]") {
    KvStore store;
    store.set("hello", Value{"world"});

    auto val = store.get("hello");
    REQUIRE(val.has_value());
    CHECK(val->as_string() == "world");
}

TEST_CASE("KvStore: get missing key returns nullopt", "[kv]") {
    KvStore store;
    CHECK_FALSE(store.get("nonexistent").has_value());
}

TEST_CASE("KvStore: overwrite existing key", "[kv]") {
    KvStore store;
    store.set("k", Value{"first"});
    store.set("k", Value{"second"});
    REQUIRE(store.get("k").has_value());
    CHECK(store.get("k")->as_string() == "second");
}

// ---------------------------------------------------------------------------
// del
// ---------------------------------------------------------------------------

TEST_CASE("KvStore: del existing key returns 1", "[kv]") {
    KvStore store;
    store.set("k", Value{"v"});
    CHECK(store.del("k") == 1);
    CHECK_FALSE(store.get("k").has_value());
}

TEST_CASE("KvStore: del missing key returns 0", "[kv]") {
    KvStore store;
    CHECK(store.del("nope") == 0);
}

// ---------------------------------------------------------------------------
// exists
// ---------------------------------------------------------------------------

TEST_CASE("KvStore: exists", "[kv]") {
    KvStore store;
    CHECK_FALSE(store.exists("k"));
    store.set("k", Value{"v"});
    CHECK(store.exists("k"));
    store.del("k");
    CHECK_FALSE(store.exists("k"));
}

// ---------------------------------------------------------------------------
// TTL / expire / persist
// ---------------------------------------------------------------------------

TEST_CASE("KvStore: ttl on key without expiry returns -1", "[kv]") {
    KvStore store;
    store.set("k", Value{"v"});
    CHECK(store.ttl("k") == -1);
}

TEST_CASE("KvStore: ttl on missing key returns -2", "[kv]") {
    KvStore store;
    CHECK(store.ttl("nope") == -2);
}

TEST_CASE("KvStore: ttl returns positive ms when expiry set", "[kv]") {
    KvStore store;
    store.set("k", Value{"v"}, 500ms);
    auto remaining = store.ttl("k");
    CHECK(remaining > 0);
    CHECK(remaining <= 500);
}

TEST_CASE("KvStore: expire attaches ttl to existing key", "[kv]") {
    KvStore store;
    store.set("k", Value{"v"});
    CHECK(store.expire("k", 500ms));
    CHECK(store.ttl("k") > 0);
}

TEST_CASE("KvStore: expire on missing key returns false", "[kv]") {
    KvStore store;
    CHECK_FALSE(store.expire("nope", 500ms));
}

TEST_CASE("KvStore: persist removes expiry", "[kv]") {
    KvStore store;
    store.set("k", Value{"v"}, 500ms);
    CHECK(store.persist("k"));
    CHECK(store.ttl("k") == -1);
}

TEST_CASE("KvStore: persist on key without expiry still returns true", "[kv]") {
    KvStore store;
    store.set("k", Value{"v"});
    CHECK(store.persist("k"));  // no-op but succeeds
}

TEST_CASE("KvStore: persist on missing key returns false", "[kv]") {
    KvStore store;
    CHECK_FALSE(store.persist("nope"));
}

// ---------------------------------------------------------------------------
// Key expiry
// ---------------------------------------------------------------------------

TEST_CASE("KvStore: key expires and becomes inaccessible", "[kv][expiry]") {
    KvStore store;
    store.set("k", Value{"v"}, 50ms);

    CHECK(store.exists("k"));

    std::this_thread::sleep_for(100ms);

    CHECK_FALSE(store.exists("k"));
    CHECK_FALSE(store.get("k").has_value());
    CHECK(store.ttl("k") == -2);
}

TEST_CASE("KvStore: del on expired key returns 0", "[kv][expiry]") {
    KvStore store;
    store.set("k", Value{"v"}, 50ms);
    std::this_thread::sleep_for(100ms);
    CHECK(store.del("k") == 0);
}

TEST_CASE("KvStore: set with ttl=0 means no expiry", "[kv]") {
    KvStore store;
    store.set("k", Value{"v"}, 0ms);
    CHECK(store.ttl("k") == -1);
}

// ---------------------------------------------------------------------------
// size
// ---------------------------------------------------------------------------

TEST_CASE("KvStore: size tracks insertions and deletions", "[kv]") {
    KvStore store;
    CHECK(store.size() == 0);
    store.set("a", Value{"1"});
    store.set("b", Value{"2"});
    CHECK(store.size() == 2);
    store.del("a");
    CHECK(store.size() == 1);
}

// ---------------------------------------------------------------------------
// Concurrency smoke test
// ---------------------------------------------------------------------------

TEST_CASE("KvStore: concurrent reads and writes don't crash", "[kv][concurrency]") {
    KvStore store;

    constexpr int kThreads = 8;
    constexpr int kOps     = 200;

    std::vector<std::jthread> threads;
    threads.reserve(static_cast<std::size_t>(kThreads));

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&store, t] {
            for (int i = 0; i < kOps; ++i) {
                std::string key = "key" + std::to_string(t * kOps + i);
                store.set(key, Value{std::to_string(i)});
                store.get(key);
                store.exists(key);
                store.del(key);
            }
        });
    }
    // threads join on destruction — no crash = pass
}
