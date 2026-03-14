#include "utils/config_loader.hpp"

#include <toml++/toml.hpp>

#include <stdexcept>
#include <string>

namespace vortek {

Config load_config_file(std::string_view path, const Config& base) {
    toml::table tbl;
    try {
        tbl = toml::parse_file(path);
    } catch (const toml::parse_error& err) {
        throw std::runtime_error("TOML parse error in '" + std::string(path) +
                                 "': " + std::string(err.description()));
    }

    Config cfg = base;

    if (auto* server = tbl["server"].as_table()) {
        if (auto v = (*server)["port"].value<int64_t>())
            cfg.port = static_cast<uint16_t>(*v);
        if (auto v = (*server)["log_level"].value<std::string>())
            cfg.log_level = *v;
    }

    if (auto* persistence = tbl["persistence"].as_table()) {
        if (auto v = (*persistence)["aof_enabled"].value<bool>())
            cfg.aof_enabled = *v;
        if (auto v = (*persistence)["aof_path"].value<std::string>())
            cfg.aof_path = *v;
    }

    return cfg;
}

}  // namespace vortek
