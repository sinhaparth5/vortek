#pragma once

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace vortek {

// Thread-safe publish/subscribe message broker.
//
// Each subscriber is identified by a unique integer id (typically the address
// of the owning Connection) and provides a write callback that is invoked on
// PUBLISH.  All callbacks are fired while the broker lock is NOT held, so
// subscribers may call back into the broker without deadlocking.
class PubSubBroker {
public:
    using WriteFn = std::function<void(std::string)>;

    // Subscribe to a channel.  Returns the total subscriber count for that
    // channel after the operation.
    int64_t subscribe(std::string_view channel, uintptr_t id, WriteFn write_fn);

    // Unsubscribe from a specific channel.  Returns the remaining subscriber
    // count for that channel after the operation.
    int64_t unsubscribe(std::string_view channel, uintptr_t id);

    // Unsubscribe from every channel.  Returns the list of channel names that
    // were removed (for building response messages).
    std::vector<std::string> unsubscribe_all(uintptr_t id);

    // Publish message to all subscribers of channel.  Returns the number of
    // subscribers that received the message.
    int64_t publish(std::string_view channel, std::string_view message);

private:
    struct Subscriber {
        uintptr_t id;
        WriteFn   write;
    };

    std::mutex mutex_;
    std::unordered_map<std::string, std::vector<Subscriber>> channels_;
};

}  // namespace vortek
