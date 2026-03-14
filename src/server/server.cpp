#include "server.hpp"

#include "connection.hpp"
#include "utils/logger.hpp"

#include <string>

namespace vortek {

Server::Server(const Config&     cfg,
               KvStore&          store,
               const Dispatcher& dispatcher,
               AofPersistence*   aof,
               ServerStats*      stats,
               PubSubBroker*     broker)
    : acceptor_(io_ctx_,
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), cfg.port))
    , store_(store)
    , dispatcher_(dispatcher)
    , aof_(aof)
    , stats_(stats)
    , broker_(broker) {
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    log::info("Vortek listening on port " + std::to_string(cfg.port));
}

void Server::run() {
    do_accept();
    io_ctx_.run();
}

void Server::stop() {
    io_ctx_.stop();
}

void Server::do_accept() {
    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                log::info("new connection from "
                          + socket.remote_endpoint().address().to_string());
                Connection::create(
                    std::move(socket), store_, dispatcher_, aof_, stats_, broker_)->start();
            } else {
                log::error("accept error: " + ec.message());
            }
            do_accept();
        });
}

}  // namespace vortek
