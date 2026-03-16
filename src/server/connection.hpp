#pragma once

#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "persistence/aof_persistence.hpp"
#include "protocol/resp_parser.hpp"
#include "server/pubsub_broker.hpp"
#include "server/server_stats.hpp"

#include <asio.hpp>

#include <array>
#include <chrono>
#include <cstddef>
#include <deque>
#include <memory>
#include <string>
#include <unordered_set>

namespace vortek {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    // aof, stats, and broker may be nullptr.
    static std::shared_ptr<Connection> create(asio::ip::tcp::socket socket,
                                              KvStore&              store,
                                              const Dispatcher&     dispatcher,
                                              AofPersistence*       aof,
                                              ServerStats*          stats,
                                              PubSubBroker*         broker,
                                              std::string           requirepass,
                                              std::size_t           max_request_bytes,
                                              std::size_t           idle_timeout_seconds,
                                              std::size_t           max_pending_write_bytes);

    void start();
    ~Connection();  // decrements stats_->connected_clients, unsubscribes from all channels

private:
    Connection(asio::ip::tcp::socket socket,
               KvStore&              store,
               const Dispatcher&     dispatcher,
               AofPersistence*       aof,
               ServerStats*          stats,
               PubSubBroker*         broker,
               std::string           requirepass,
               std::size_t           max_request_bytes,
               std::size_t           idle_timeout_seconds,
               std::size_t           max_pending_write_bytes);

    void do_read();
    void handle_data(std::size_t bytes);
    RespValue handle_auth(const Command& cmd);
    void handle_subscribe(const Command& cmd);
    void handle_unsubscribe(const Command& cmd);
    void push_message(std::string_view channel, std::string message);
    void do_write(std::string data);
    void flush_write_queue();
    void close_socket();
    void reset_idle_timer();

    asio::ip::tcp::socket        socket_;
    asio::steady_timer           idle_timer_;
    KvStore&                     store_;
    const Dispatcher&            dispatcher_;
    AofPersistence*              aof_;     // non-owning, may be null
    ServerStats*                 stats_;   // non-owning, may be null
    PubSubBroker*                broker_;  // non-owning, may be null
    RespParser                   parser_;
    std::array<char, 4096>       read_buf_;
    std::string                  write_buf_;
    std::deque<std::string>      write_queue_;
    std::string                  requirepass_;
    std::size_t                  max_request_bytes_;
    std::chrono::seconds         idle_timeout_;
    std::size_t                  max_pending_write_bytes_;
    std::size_t                  pending_write_bytes_ = 0;
    bool                         authenticated_       = false;
    bool                         close_after_write_   = false;
    bool                         write_in_progress_   = false;
    bool                         read_in_progress_    = false;
    std::unordered_set<std::string> subscribed_channels_;
};

}  // namespace vortek
