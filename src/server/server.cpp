#include "server.hpp"

#include "connection.hpp"
#include "protocol/resp_serializer.hpp"
#include "utils/logger.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <string>

namespace vortek {
namespace {
std::atomic_int g_last_signal{0};

extern "C" void vortek_signal_handler(int signal_number) {
    g_last_signal.store(signal_number, std::memory_order_relaxed);
}
}  // namespace

Server::Server(const Config&     cfg,
               KvStore&          store,
               const Dispatcher& dispatcher,
               AofPersistence*   aof,
               ServerStats*      stats,
               PubSubBroker*     broker)
    : acceptor_(io_ctx_,
                asio::ip::tcp::endpoint(asio::ip::tcp::v4(), cfg.port))
    , signal_poll_timer_(io_ctx_)
    , requirepass_(cfg.requirepass)
    , max_request_bytes_(cfg.max_request_bytes)
    , idle_timeout_seconds_(cfg.idle_timeout_seconds)
    , max_clients_(cfg.max_clients)
    , max_pending_write_bytes_(cfg.max_pending_write_bytes)
    , store_(store)
    , dispatcher_(dispatcher)
    , aof_(aof)
    , stats_(stats)
    , broker_(broker) {
    acceptor_.set_option(asio::ip::tcp::acceptor::reuse_address(true));
    log::info("Vortek listening on port " + std::to_string(cfg.port));
}

void Server::run() {
    setup_signal_handlers();
    do_accept();
    io_ctx_.run();
}

void Server::stop() {
    if (stopping_.exchange(true))
        return;

    asio::error_code ignored_ec;
    acceptor_.cancel(ignored_ec);
    acceptor_.close(ignored_ec);
    signal_poll_timer_.cancel(ignored_ec);
    io_ctx_.stop();
}

void Server::setup_signal_handlers() {
    g_last_signal.store(0, std::memory_order_relaxed);
    std::signal(SIGINT, vortek_signal_handler);
    std::signal(SIGTERM, vortek_signal_handler);
    poll_signal_flag();
}

void Server::poll_signal_flag() {
    if (stopping_.load())
        return;

    const int signal_number = g_last_signal.load(std::memory_order_relaxed);
    if (signal_number != 0) {
        log::info("received signal " + std::to_string(signal_number) + ", shutting down");
        stop();
        return;
    }

    signal_poll_timer_.expires_after(std::chrono::milliseconds(100));
    signal_poll_timer_.async_wait([this](const asio::error_code& ec) {
        if (ec == asio::error::operation_aborted || stopping_.load())
            return;
        if (ec) {
            log::error("signal poll timer error: " + ec.message());
            return;
        }
        poll_signal_flag();
    });
}

void Server::do_accept() {
    acceptor_.async_accept(
        [this](const asio::error_code& ec, asio::ip::tcp::socket socket) {
            if (!ec) {
                const int64_t current_clients = stats_
                    ? stats_->connected_clients.load(std::memory_order_relaxed)
                    : 0;
                if (stats_ && current_clients >= static_cast<int64_t>(max_clients_)) {
                    const auto err = serialize(
                        RespError{"ERR max number of clients reached"});
                    asio::error_code write_ec;
                    asio::write(socket, asio::buffer(err), write_ec);
                    asio::error_code ignored_ec;
                    socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
                    socket.close(ignored_ec);
                    log::warn("connection rejected: max clients reached");
                    do_accept();
                    return;
                }

                log::info("new connection from "
                          + socket.remote_endpoint().address().to_string());
                Connection::create(
                    std::move(socket),
                    store_,
                    dispatcher_,
                    aof_,
                    stats_,
                    broker_,
                    requirepass_,
                    max_request_bytes_,
                    idle_timeout_seconds_,
                    max_pending_write_bytes_)
                    ->start();
            } else if (ec == asio::error::operation_aborted || stopping_.load()) {
                return;
            } else {
                log::error("accept error: " + ec.message());
            }
            do_accept();
        });
}

}  // namespace vortek
