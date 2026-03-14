#pragma once

#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "utils/config.hpp"

#include <asio.hpp>

namespace vortek {

// TCP server — owns the io_context and the acceptor.
// Call run() to start accepting (blocks until stop() is called).
class Server {
public:
    Server(const Config& cfg, KvStore& store, const Dispatcher& dispatcher);

    // Start the accept loop and run the event loop (blocks).
    void run();

    // Signal the event loop to stop gracefully.
    void stop();

private:
    void do_accept();

    asio::io_context        io_ctx_;
    asio::ip::tcp::acceptor acceptor_;
    KvStore&                store_;
    const Dispatcher&       dispatcher_;
};

}  // namespace vortek
