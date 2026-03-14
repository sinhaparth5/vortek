#include "connection.hpp"

#include "commands/command.hpp"
#include "protocol/resp_serializer.hpp"
#include "utils/logger.hpp"

namespace vortek {

// ---------------------------------------------------------------------------
// Factory + constructor
// ---------------------------------------------------------------------------

std::shared_ptr<Connection> Connection::create(asio::ip::tcp::socket socket,
                                               KvStore&              store,
                                               const Dispatcher&     dispatcher) {
    // Can't use make_shared because the constructor is private.
    return std::shared_ptr<Connection>(
        new Connection(std::move(socket), store, dispatcher));
}

Connection::Connection(asio::ip::tcp::socket socket,
                       KvStore&              store,
                       const Dispatcher&     dispatcher)
    : socket_(std::move(socket)), store_(store), dispatcher_(dispatcher) {}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void Connection::start() {
    do_read();
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void Connection::do_read() {
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(read_buf_),
        [this, self](const asio::error_code& ec, std::size_t bytes) {
            if (ec) {
                // EOF or connection reset — let the shared_ptr expire naturally.
                if (ec != asio::error::eof && ec != asio::error::connection_reset)
                    log::error("read error: " + ec.message());
                return;
            }
            handle_data(bytes);
        });
}

void Connection::handle_data(std::size_t bytes) {
    std::string responses;

    try {
        // Feed new bytes into the parser, then drain all complete messages
        // (one network read may contain several pipelined commands).
        std::string_view chunk(read_buf_.data(), bytes);
        bool             first = true;

        while (true) {
            auto val = parser_.feed(first ? chunk : std::string_view{});
            first    = false;
            if (!val) break;

            auto cmd = parse_command(*val);
            if (cmd) {
                responses += serialize(dispatcher_.dispatch(*cmd, store_));
            } else {
                responses += serialize(
                    RespError{"ERR protocol error: expected command array"});
                parser_.reset();
                break;
            }
        }
    } catch (const std::exception& ex) {
        responses += serialize(RespError{"ERR " + std::string(ex.what())});
        parser_.reset();
    }

    if (!responses.empty())
        do_write(std::move(responses));
    else
        do_read();
}

void Connection::do_write(std::string data) {
    write_buf_  = std::move(data);
    auto self   = shared_from_this();
    asio::async_write(
        socket_,
        asio::buffer(write_buf_),
        [this, self](const asio::error_code& ec, std::size_t /*bytes*/) {
            if (ec) {
                log::error("write error: " + ec.message());
                return;
            }
            do_read();
        });
}

}  // namespace vortek
