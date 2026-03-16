#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "persistence/aof_persistence.hpp"
#include "server/pubsub_broker.hpp"
#include "server/server.hpp"
#include "server/server_stats.hpp"
#include "utils/config.hpp"
#include "utils/config_loader.hpp"
#include "utils/logger.hpp"

#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>

static void print_usage(const char* prog) {
    std::printf(
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --config <path>     Load settings from a TOML config file\n"
        "  --port <n>          TCP port to listen on        (default: 6379)\n"
        "  --aof <path>        Enable AOF and set file path (default: vortek.aof)\n"
        "  --no-aof            Disable AOF persistence\n"
        "  --requirepass <pw>  Require AUTH with this password\n"
        "  --log-level <lvl>   debug | info | warn | error  (default: info)\n"
        "  --max-request-bytes <n>  Max buffered request bytes per connection\n"
        "  --idle-timeout-seconds <n>  Close idle connections after n seconds\n"
        "  --max-clients <n>   Maximum concurrent client connections\n"
        "  --max-pending-write-bytes <n>  Max buffered reply bytes per connection\n"
        "  --help              Show this help message\n"
        "\n"
        "CLI options override values from --config.\n",
        prog);
}

static vortek::Config parse_args(int argc, char* argv[]) {
    // First pass: find --config so we can load it before applying CLI overrides.
    std::optional<std::string> config_path;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];
        if (arg == "--config" && i + 1 < argc)
            config_path = argv[++i];
    }

    vortek::Config cfg;
    if (config_path)
        cfg = vortek::load_config_file(*config_path);

    // Second pass: apply individual CLI overrides on top of the loaded config.
    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        auto need_next = [&]() -> std::string_view {
            if (i + 1 >= argc)
                throw std::runtime_error(std::string(arg) + " requires an argument");
            return argv[++i];
        };

        if (arg == "--config") {
            ++i;  // already consumed above
        } else if (arg == "--port") {
            cfg.port = static_cast<uint16_t>(std::stoi(std::string(need_next())));
        } else if (arg == "--aof") {
            cfg.aof_path    = std::string(need_next());
            cfg.aof_enabled = true;
        } else if (arg == "--no-aof") {
            cfg.aof_enabled = false;
        } else if (arg == "--requirepass") {
            cfg.requirepass = std::string(need_next());
        } else if (arg == "--log-level") {
            cfg.log_level = std::string(need_next());
        } else if (arg == "--max-request-bytes") {
            cfg.max_request_bytes
                = static_cast<std::size_t>(std::stoull(std::string(need_next())));
        } else if (arg == "--idle-timeout-seconds") {
            cfg.idle_timeout_seconds
                = static_cast<std::size_t>(std::stoull(std::string(need_next())));
        } else if (arg == "--max-clients") {
            cfg.max_clients = static_cast<std::size_t>(std::stoull(std::string(need_next())));
        } else if (arg == "--max-pending-write-bytes") {
            cfg.max_pending_write_bytes
                = static_cast<std::size_t>(std::stoull(std::string(need_next())));
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + std::string(arg));
        }
    }

    return cfg;
}

int main(int argc, char* argv[]) {
    vortek::Config cfg;
    try {
        cfg = parse_args(argc, argv);
    } catch (const std::exception& ex) {
        std::fprintf(stderr, "error: %s\n", ex.what());
        print_usage(argv[0]);
        return 1;
    }

    vortek::log::set_level(cfg.log_level);
    vortek::log::info("Vortek v0.1.0 starting");

    // Stats — populated before dispatcher so INFO handler sees the right values.
    vortek::ServerStats stats;
    stats.port        = cfg.port;
    stats.aof_enabled = cfg.aof_enabled;

    vortek::KvStore      store;
    vortek::PubSubBroker broker;
    auto dispatcher = vortek::Dispatcher::make_default(stats, &broker);

    std::unique_ptr<vortek::AofPersistence> aof;
    if (cfg.aof_enabled) {
        aof = std::make_unique<vortek::AofPersistence>(cfg.aof_path);
        if (aof->is_open())
            aof->replay(dispatcher, store);
        else
            aof.reset();
    }

    vortek::Server server(cfg, store, dispatcher, aof.get(), &stats, &broker);
    server.run();
    return 0;
}
