#include <catch2/catch_all.hpp>

#include "utils/config_loader.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace vortek;

namespace {
std::string make_temp_config_path() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path()
            / ("vortek-config-test-" + std::to_string(now) + ".toml"))
        .string();
}
}  // namespace

TEST_CASE("Config loader reads connection limits", "[config]") {
    const std::string path = make_temp_config_path();
    {
        std::ofstream out(path);
        REQUIRE(out.is_open());
        out << "[server]\n";
        out << "port = 6380\n";
        out << "log_level = \"debug\"\n";
        out << "max_clients = 123\n";
        out << "max_pending_write_bytes = 65536\n";
    }

    const Config cfg = load_config_file(path);
    CHECK(cfg.port == 6380);
    CHECK(cfg.log_level == "debug");
    CHECK(cfg.max_clients == 123);
    CHECK(cfg.max_pending_write_bytes == 65536);

    std::filesystem::remove(path);
}
