#include "pubsub_broker.hpp"

#include <algorithm>

namespace vortek {

int64_t PubSubBroker::subscribe(std::string_view channel, uintptr_t id, WriteFn write_fn) {
    std::lock_guard lock(mutex_);
    auto& subs = channels_[std::string(channel)];
    // Replace if already subscribed (idempotent re-subscribe)
    for (auto& s : subs) {
        if (s.id == id) {
            s.write = std::move(write_fn);
            return static_cast<int64_t>(subs.size());
        }
    }
    subs.push_back({id, std::move(write_fn)});
    return static_cast<int64_t>(subs.size());
}

int64_t PubSubBroker::unsubscribe(std::string_view channel, uintptr_t id) {
    std::lock_guard lock(mutex_);
    auto it = channels_.find(channel);
    if (it == channels_.end()) return 0;

    auto& subs = it->second;
    subs.erase(std::remove_if(subs.begin(), subs.end(),
                               [id](const Subscriber& s) { return s.id == id; }),
               subs.end());

    const auto count = static_cast<int64_t>(subs.size());
    if (subs.empty()) channels_.erase(it);
    return count;
}

std::vector<std::string> PubSubBroker::unsubscribe_all(uintptr_t id) {
    std::lock_guard lock(mutex_);
    std::vector<std::string> removed;

    for (auto it = channels_.begin(); it != channels_.end(); ) {
        auto& subs = it->second;
        const auto before = subs.size();
        subs.erase(std::remove_if(subs.begin(), subs.end(),
                                   [id](const Subscriber& s) { return s.id == id; }),
                   subs.end());
        if (subs.size() != before)
            removed.push_back(it->first);

        if (subs.empty())
            it = channels_.erase(it);
        else
            ++it;
    }
    return removed;
}

int64_t PubSubBroker::publish(std::string_view channel, std::string_view message) {
    // Snapshot subscribers so we can call write callbacks without holding the lock.
    std::vector<WriteFn> callbacks;
    {
        std::lock_guard lock(mutex_);
        auto it = channels_.find(channel);
        if (it == channels_.end()) return 0;
        callbacks.reserve(it->second.size());
        for (const auto& s : it->second)
            callbacks.push_back(s.write);
    }

    for (auto& fn : callbacks)
        fn(std::string(message));

    return static_cast<int64_t>(callbacks.size());
}

}  // namespace vortek
