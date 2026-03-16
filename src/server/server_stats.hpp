#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>

namespace vortek {

// Shared, thread-safe server metrics.
// Owned by main(), outlives all connections and the dispatcher.
struct ServerStats {
    using Clock = std::chrono::steady_clock;

    Clock::time_point     start_time{Clock::now()};
    std::atomic<int64_t>  connected_clients{0};
    std::atomic<int64_t>  total_commands{0};
    std::atomic<bool>     ready{false};

    // Filled in from Config before the server starts accepting.
    uint16_t    port        = 6379;
    bool        aof_enabled = false;

    int64_t uptime_seconds() const {
        auto dur = Clock::now() - start_time;
        return std::chrono::duration_cast<std::chrono::seconds>(dur).count();
    }
};

}  // namespace vortek
