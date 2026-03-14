#pragma once

#include "utils/config.hpp"

#include <string_view>

namespace vortek {

// Load a Config from a TOML file.  Only keys present in the file are applied;
// absent keys keep their default values from `base`.
// Throws std::runtime_error if the file cannot be opened or parsed.
Config load_config_file(std::string_view path, const Config& base = {});

}  // namespace vortek
