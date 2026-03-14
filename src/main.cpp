#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "server/server.hpp"
#include "utils/config.hpp"
#include "utils/logger.hpp"

int main() {
    vortek::Config cfg;
    cfg.port = 6379;

    vortek::KvStore    store;
    auto               dispatcher = vortek::Dispatcher::make_default();
    vortek::Server     server(cfg, store, dispatcher);

    server.run();
    return 0;
}
