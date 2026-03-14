#include "kv_store.hpp"

#include <algorithm>
#include <mutex>

namespace vortek {

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

KvStore::KvStore()
    : expiry_thread_([this](std::stop_token st) { run_expiry(st); }) {}

KvStore::~KvStore() = default;  // jthread destructor calls request_stop() + join()

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void KvStore::set(std::string_view key, Value value, std::chrono::milliseconds ttl) {
    std::optional<TimePoint> expires_at;
    if (ttl.count() > 0)
        expires_at = Clock::now() + ttl;

    std::unique_lock lock(mutex_);
    data_.insert_or_assign(std::string(key), Entry{std::move(value), expires_at});
}

std::optional<Value> KvStore::get(std::string_view key) const {
    std::shared_lock lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end() || it->second.is_expired())
        return std::nullopt;
    return it->second.value;
}

int KvStore::del(std::string_view key) {
    std::unique_lock lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end())
        return 0;
    // Clean up even if expired, but report it as absent.
    const bool alive = !it->second.is_expired();
    data_.erase(it);
    return alive ? 1 : 0;
}

bool KvStore::exists(std::string_view key) const {
    std::shared_lock lock(mutex_);
    auto it = data_.find(key);
    return it != data_.end() && !it->second.is_expired();
}

bool KvStore::expire(std::string_view key, std::chrono::milliseconds ttl) {
    std::unique_lock lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end() || it->second.is_expired())
        return false;
    it->second.expires_at = Clock::now() + ttl;
    return true;
}

int64_t KvStore::ttl(std::string_view key) const {
    std::shared_lock lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end() || it->second.is_expired())
        return -2;
    if (!it->second.expires_at.has_value())
        return -1;

    auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
        *it->second.expires_at - Clock::now());
    return std::max(int64_t{0}, remaining.count());
}

bool KvStore::persist(std::string_view key) {
    std::unique_lock lock(mutex_);
    auto it = data_.find(key);
    if (it == data_.end() || it->second.is_expired())
        return false;
    it->second.expires_at.reset();
    return true;
}

std::size_t KvStore::size() const {
    std::shared_lock lock(mutex_);
    return data_.size();
}

// ---------------------------------------------------------------------------
// Background expiry sweep
// ---------------------------------------------------------------------------

void KvStore::run_expiry(std::stop_token stop) {
    while (!stop.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        std::unique_lock lock(mutex_);
        const auto now = Clock::now();
        for (auto it = data_.begin(); it != data_.end();) {
            if (it->second.expires_at && *it->second.expires_at <= now)
                it = data_.erase(it);
            else
                ++it;
        }
    }
}

}  // namespace vortek
