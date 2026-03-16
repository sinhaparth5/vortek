#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace vortek {

struct Config {
    uint16_t    port        = 6379;
    bool        aof_enabled = true;
    std::string aof_path    = "vortek.aof";
    std::string log_level   = "info";  // debug | info | warn | error
    std::size_t max_clients = 10000;
    std::size_t max_pending_write_bytes = 1024 * 1024;
};

}  // namespace vortek
