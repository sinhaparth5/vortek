#include "connection.hpp"

#include "commands/command.hpp"
#include "protocol/resp_serializer.hpp"
#include "utils/logger.hpp"

namespace vortek {

// ---------------------------------------------------------------------------
// Helpers — build RESP push frames
// ---------------------------------------------------------------------------

// *3\r\n$9\r\nsubscribe\r\n$<ch_len>\r\n<channel>\r\n:<count>\r\n
static std::string make_subscribe_reply(std::string_view kind,
                                        std::string_view channel,
                                        int64_t          count) {
    RespArray arr;
    arr.elements.push_back(RespBulkString{std::string(kind)});
    arr.elements.push_back(RespBulkString{std::string(channel)});
    arr.elements.push_back(RespInteger{count});
    return serialize(arr);
}

// *3\r\n$7\r\nmessage\r\n$<ch_len>\r\n<channel>\r\n$<msg_len>\r\n<msg>\r\n
static std::string make_message_frame(std::string_view channel,
                                      std::string_view message) {
    RespArray arr;
    arr.elements.push_back(RespBulkString{"message"});
    arr.elements.push_back(RespBulkString{std::string(channel)});
    arr.elements.push_back(RespBulkString{std::string(message)});
    return serialize(arr);
}

// ---------------------------------------------------------------------------
// Factory + constructor/destructor
// ---------------------------------------------------------------------------

std::shared_ptr<Connection> Connection::create(asio::ip::tcp::socket socket,
                                               KvStore&              store,
                                               const Dispatcher&     dispatcher,
                                               AofPersistence*       aof,
                                               ServerStats*          stats,
                                               PubSubBroker*         broker,
                                               std::string           requirepass,
                                               std::size_t           max_request_bytes,
                                               std::size_t           idle_timeout_seconds,
                                               std::size_t           max_pending_write_bytes) {
    return std::shared_ptr<Connection>(
        new Connection(std::move(socket),
                       store,
                       dispatcher,
                       aof,
                       stats,
                       broker,
                       std::move(requirepass),
                       max_request_bytes,
                       idle_timeout_seconds,
                       max_pending_write_bytes));
}

Connection::Connection(asio::ip::tcp::socket socket,
                       KvStore&              store,
                       const Dispatcher&     dispatcher,
                       AofPersistence*       aof,
                       ServerStats*          stats,
                       PubSubBroker*         broker,
                       std::string           requirepass,
                       std::size_t           max_request_bytes,
                       std::size_t           idle_timeout_seconds,
                       std::size_t           max_pending_write_bytes)
    : socket_(std::move(socket))
    , idle_timer_(socket_.get_executor())
    , store_(store)
    , dispatcher_(dispatcher)
    , aof_(aof)
    , stats_(stats)
    , broker_(broker)
    , requirepass_(std::move(requirepass))
    , max_request_bytes_(max_request_bytes)
    , idle_timeout_(std::chrono::seconds(idle_timeout_seconds))
    , max_pending_write_bytes_(max_pending_write_bytes) {
    authenticated_ = requirepass_.empty();
    if (stats_)
        stats_->connected_clients.fetch_add(1, std::memory_order_relaxed);
}

Connection::~Connection() {
    if (stats_)
        stats_->connected_clients.fetch_sub(1, std::memory_order_relaxed);

    if (broker_ && !subscribed_channels_.empty()) {
        const auto id = reinterpret_cast<uintptr_t>(this);
        broker_->unsubscribe_all(id);
    }
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void Connection::start() {
    reset_idle_timer();
    do_read();
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void Connection::do_read() {
    if (read_in_progress_ || !socket_.is_open())
        return;

    read_in_progress_ = true;
    auto self = shared_from_this();
    socket_.async_read_some(
        asio::buffer(read_buf_),
        [this, self](const asio::error_code& ec, std::size_t bytes) {
            read_in_progress_ = false;
            if (ec) {
                if (ec != asio::error::eof && ec != asio::error::connection_reset)
                    log::error("read error: " + ec.message());
                close_socket();
                return;
            }
            reset_idle_timer();
            handle_data(bytes);
        });
}

void Connection::handle_data(std::size_t bytes) {
    std::string responses;

    try {
        if (parser_.buffered_size() + bytes > max_request_bytes_) {
            responses += serialize(RespError{"ERR max request size exceeded"});
            close_after_write_ = true;
            parser_.reset();
            do_write(std::move(responses));
            return;
        }

        std::string_view chunk(read_buf_.data(), bytes);
        bool             first = true;

        while (true) {
            auto val = parser_.feed(first ? chunk : std::string_view{});
            first    = false;
            if (!val) break;

            auto cmd = parse_command(*val);
            if (!cmd) {
                responses += serialize(
                    RespError{"ERR protocol error: expected command array"});
                parser_.reset();
                break;
            }

            if (cmd->name == "AUTH") {
                responses += serialize(handle_auth(*cmd));
                continue;
            }

            if (!authenticated_) {
                responses += serialize(RespError{"NOAUTH Authentication required."});
                continue;
            }

            if (cmd->name == "SUBSCRIBE") {
                handle_subscribe(*cmd);
                // Responses written inside handle_subscribe via do_write;
                // skip accumulation here.
                continue;
            }

            if (cmd->name == "UNSUBSCRIBE") {
                handle_unsubscribe(*cmd);
                continue;
            }

            if (aof_ && AofPersistence::is_write_command(cmd->name))
                aof_->log(*cmd);

            responses += serialize(dispatcher_.dispatch(*cmd, store_));

            if (stats_)
                stats_->total_commands.fetch_add(1, std::memory_order_relaxed);
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

RespValue Connection::handle_auth(const Command& cmd) {
    if (cmd.args.size() != 1)
        return RespError{"ERR wrong number of arguments for 'AUTH' command"};
    if (requirepass_.empty())
        return RespError{"ERR AUTH called without any password configured"};
    if (cmd.args[0] != requirepass_)
        return RespError{"ERR invalid password"};

    authenticated_ = true;
    return RespSimpleString{"OK"};
}

void Connection::handle_subscribe(const Command& cmd) {
    if (!broker_ || cmd.args.empty()) {
        do_write(serialize(RespError{"ERR SUBSCRIBE requires at least one channel"}));
        return;
    }

    const auto id   = reinterpret_cast<uintptr_t>(this);
    auto       self = shared_from_this();

    std::string replies;
    for (const auto& channel : cmd.args) {
        // Capture channel by value so the lambda outlives this stack frame.
        auto write_fn = [self, ch = channel](std::string msg) {
            self->push_message(ch, std::move(msg));
        };

        const int64_t count = broker_->subscribe(channel, id, std::move(write_fn));
        subscribed_channels_.insert(channel);
        replies += make_subscribe_reply("subscribe", channel, count);
    }

    do_write(std::move(replies));
}

void Connection::handle_unsubscribe(const Command& cmd) {
    if (!broker_) {
        do_read();
        return;
    }

    const auto id = reinterpret_cast<uintptr_t>(this);
    std::string replies;

    if (cmd.args.empty()) {
        // UNSUBSCRIBE with no args → leave all subscribed channels.
        auto channels = broker_->unsubscribe_all(id);
        for (const auto& ch : channels) {
            subscribed_channels_.erase(ch);
            replies += make_subscribe_reply("unsubscribe", ch, 0);
        }
        if (channels.empty())
            replies += make_subscribe_reply("unsubscribe", "", 0);
    } else {
        for (const auto& channel : cmd.args) {
            const int64_t count = broker_->unsubscribe(channel, id);
            subscribed_channels_.erase(channel);
            replies += make_subscribe_reply("unsubscribe", channel, count);
        }
    }

    do_write(std::move(replies));
}

void Connection::push_message(std::string_view channel, std::string message) {
    // Called from PubSubBroker::publish — may run on any thread that calls
    // PUBLISH.  Since Asio's io_context is single-threaded here, post() ensures
    // the write happens safely on the event loop.
    auto self  = shared_from_this();
    auto frame = make_message_frame(channel, message);
    asio::post(socket_.get_executor(),
               [self, data = std::move(frame)]() mutable {
                   self->do_write(std::move(data));
               });
}

void Connection::do_write(std::string data) {
    if (!socket_.is_open())
        return;

    if (pending_write_bytes_ + data.size() > max_pending_write_bytes_) {
        log::warn("closing slow client due to output buffer limit");
        close_socket();
        return;
    }

    pending_write_bytes_ += data.size();
    write_queue_.push_back(std::move(data));
    if (write_in_progress_)
        return;
    flush_write_queue();
}

void Connection::flush_write_queue() {
    if (write_queue_.empty() || !socket_.is_open()) {
        write_in_progress_ = false;
        return;
    }

    write_in_progress_ = true;
    write_buf_         = std::move(write_queue_.front());
    write_queue_.pop_front();

    auto self  = shared_from_this();
    asio::async_write(
        socket_,
        asio::buffer(write_buf_),
        [this, self](const asio::error_code& ec, std::size_t bytes) {
            if (ec) {
                log::error("write error: " + ec.message());
                close_socket();
                return;
            }
            if (pending_write_bytes_ >= bytes)
                pending_write_bytes_ -= bytes;
            else
                pending_write_bytes_ = 0;

            if (!write_queue_.empty()) {
                flush_write_queue();
                return;
            }

            write_in_progress_ = false;
            if (close_after_write_) {
                close_socket();
                return;
            }
            reset_idle_timer();
            do_read();
        }
    );
}

void Connection::close_socket() {
    idle_timer_.cancel();
    asio::error_code ignored_ec;
    socket_.shutdown(asio::ip::tcp::socket::shutdown_both, ignored_ec);
    socket_.close(ignored_ec);
}

void Connection::reset_idle_timer() {
    if (idle_timeout_.count() == 0 || !socket_.is_open())
        return;

    idle_timer_.expires_after(idle_timeout_);
    auto self = shared_from_this();
    idle_timer_.async_wait([this, self](const asio::error_code& ec) {
        if (ec == asio::error::operation_aborted || !socket_.is_open())
            return;
        if (ec) {
            log::error("idle timer error: " + ec.message());
            return;
        }
        log::warn("closing idle connection");
        close_socket();
    });
}

}  // namespace vortek
