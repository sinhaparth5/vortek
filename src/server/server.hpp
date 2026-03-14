#pragma once

#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "persistence/aof_persistence.hpp"
#include "server/pubsub_broker.hpp"
#include "server/server_stats.hpp"
#include "utils/config.hpp"

#include <asio.hpp>

namespace vortek {

class Server {
public:
    Server(const Config&     cfg,
           KvStore&          store,
           const Dispatcher& dispatcher,
           AofPersistence*   aof    = nullptr,
           ServerStats*      stats  = nullptr,
           PubSubBroker*     broker = nullptr);

    void run();
    void stop();

private:
    void do_accept();

    asio::io_context        io_ctx_;
    asio::ip::tcp::acceptor acceptor_;
    KvStore&                store_;
    const Dispatcher&       dispatcher_;
    AofPersistence*         aof_;
    ServerStats*            stats_;
    PubSubBroker*           broker_;
};

}  // namespace vortek
