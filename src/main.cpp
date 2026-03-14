#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "persistence/aof_persistence.hpp"
#include "server/server.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"

#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string_view>

// ---------------------------------------------------------------------------
// CLI argument parsing
// ---------------------------------------------------------------------------

static void print_usage(const char* prog) {
    std::printf(
        "Usage: %s [options]\n"
        "\n"
        "Options:\n"
        "  --port <n>          TCP port to listen on       (default: 6379)\n"
        "  --aof <path>        Enable AOF and set file path (default: vortek.aof)\n"
        "  --no-aof            Disable AOF persistence\n"
        "  --log-level <lvl>   debug | info | warn | error  (default: info)\n"
        "  --help              Show this help message\n",
        prog);
}

static vortek::Config parse_args(int argc, char* argv[]) {
    vortek::Config cfg;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg = argv[i];

        auto need_next = [&]() -> std::string_view {
            if (i + 1 >= argc)
                throw std::runtime_error(std::string(arg) + " requires an argument");
            return argv[++i];
        };

        if (arg == "--port") {
            cfg.port = static_cast<uint16_t>(std::stoi(std::string(need_next())));
        } else if (arg == "--aof") {
            cfg.aof_path    = std::string(need_next());
            cfg.aof_enabled = true;
        } else if (arg == "--no-aof") {
            cfg.aof_enabled = false;
        } else if (arg == "--log-level") {
            cfg.log_level = std::string(need_next());
        } else if (arg == "--help" || arg == "-h") {
            print_usage(argv[0]);
            std::exit(0);
        } else {
            throw std::runtime_error("unknown option: " + std::string(arg));
        }
    }

    return cfg;
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

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

    vortek::KvStore store;
    auto            dispatcher = vortek::Dispatcher::make_default();

    std::unique_ptr<vortek::AofPersistence> aof;
    if (cfg.aof_enabled) {
        aof = std::make_unique<vortek::AofPersistence>(cfg.aof_path);
        if (aof->is_open())
            aof->replay(dispatcher, store);
        else
            aof.reset();
    }

    vortek::Server server(cfg, store, dispatcher, aof.get());
    server.run();
    return 0;
}
