#pragma once

#include <cstdint>
#include <string>

namespace vortek {

struct Config {
    uint16_t    port        = 6379;
    bool        aof_enabled = true;
    std::string aof_path    = "vortek.aof";
    std::string log_level   = "info";  // debug | info | warn | error
};

}  // namespace vortek
