#pragma once

#include "value.hpp"

#include <chrono>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>

namespace vortek {

// Thread-safe in-memory key-value store.
//
// Concurrency model:
//   - Reads use std::shared_lock  (multiple readers allowed concurrently).
//   - Writes use std::unique_lock (exclusive).
//
// Expiry model (mirrors Redis):
//   - Lazy:       expired keys are treated as absent on every read/write.
//   - Background: a jthread sweeps for expired keys every 100 ms.
class KvStore {
public:
    KvStore();
    ~KvStore();  // jthread requests stop and joins automatically

    KvStore(const KvStore&)            = delete;
    KvStore& operator=(const KvStore&) = delete;

    // Set key → value with an optional TTL (0 = no expiry).
    void set(std::string_view key, Value value,
             std::chrono::milliseconds ttl = std::chrono::milliseconds{0});

    // Returns the value, or nullopt if the key is missing or expired.
    std::optional<Value> get(std::string_view key) const;

    // Delete a key. Returns 1 if it existed and was not expired, 0 otherwise.
    int del(std::string_view key);

    // True if the key exists and has not expired.
    bool exists(std::string_view key) const;

    // Attach / replace a TTL on an existing key.
    // Returns false if the key is missing or expired.
    bool expire(std::string_view key, std::chrono::milliseconds ttl);

    // Remaining TTL in milliseconds.
    // Returns -1 if the key has no expiry, -2 if it doesn't exist.
    int64_t ttl(std::string_view key) const;

    // Remove the TTL from a key (make it persistent).
    // Returns false if the key is missing or expired.
    bool persist(std::string_view key);

    // Number of keys currently in the store (includes not-yet-swept expired keys).
    std::size_t size() const;

private:
    using Clock     = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    struct Entry {
        Value                    value;
        std::optional<TimePoint> expires_at;

        bool is_expired() const noexcept {
            return expires_at.has_value() && Clock::now() >= *expires_at;
        }
    };

    // Transparent hash/equal so find() accepts string_view without allocation.
    struct StringHash {
        using is_transparent = void;
        std::size_t operator()(std::string_view sv) const noexcept {
            return std::hash<std::string_view>{}(sv);
        }
    };
    struct StringEqual {
        using is_transparent = void;
        bool operator()(std::string_view a, std::string_view b) const noexcept {
            return a == b;
        }
    };

    mutable std::shared_mutex mutex_;
    std::unordered_map<std::string, Entry, StringHash, StringEqual> data_;
    std::jthread expiry_thread_;

    void run_expiry(std::stop_token stop);
};

}  // namespace vortek
