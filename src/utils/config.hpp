#pragma once

#include <cstdint>
#include <string>

namespace vortek {

struct Config {
    uint16_t    port     = 6379;
    std::string aof_path;   // empty = AOF disabled
    bool        aof_enabled = false;
};

}  // namespace vortek
