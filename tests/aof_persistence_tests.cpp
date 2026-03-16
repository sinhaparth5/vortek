#include <catch2/catch_all.hpp>

#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "persistence/aof_persistence.hpp"
#include "server/server_stats.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>

using namespace vortek;

namespace {
std::string make_temp_aof_path() {
    const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    return (std::filesystem::temp_directory_path()
            / ("vortek-aof-test-" + std::to_string(now) + ".aof"))
        .string();
}
}  // namespace

TEST_CASE("AOF replay truncates incomplete trailing command", "[aof][replay]") {
    const std::string path = make_temp_aof_path();
    {
        ServerStats stats;
        Dispatcher dispatcher = Dispatcher::make_default(stats);
        KvStore store;
        AofPersistence aof(path);
        REQUIRE(aof.is_open());

        aof.log(Command{"SET", {"k1", "v1"}});
        aof.log(Command{"SET", {"k2", "v2"}});

        std::ofstream out(path, std::ios::app | std::ios::binary);
        REQUIRE(out.is_open());
        out << "*2\r\n$3\r\nGET\r\n$";
    }

    ServerStats replay_stats;
    Dispatcher replay_dispatcher = Dispatcher::make_default(replay_stats);
    KvStore replay_store;
    AofPersistence replay_aof(path);
    REQUIRE(replay_aof.is_open());

    const std::size_t replayed = replay_aof.replay(replay_dispatcher, replay_store);
    CHECK(replayed == 2);

    auto k1 = replay_store.get("k1");
    auto k2 = replay_store.get("k2");
    REQUIRE(k1.has_value());
    REQUIRE(k2.has_value());
    CHECK(k1->as_string() == "v1");
    CHECK(k2->as_string() == "v2");

    std::ifstream in(path, std::ios::binary);
    REQUIRE(in.is_open());
    const std::string content((std::istreambuf_iterator<char>(in)),
                              std::istreambuf_iterator<char>{});
    CHECK(content.find("*2\r\n$3\r\nGET\r\n$") == std::string::npos);

    std::filesystem::remove(path);
}
