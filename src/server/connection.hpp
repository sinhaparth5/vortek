#pragma once

#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "persistence/aof_persistence.hpp"
#include "protocol/resp_parser.hpp"
#include "server/pubsub_broker.hpp"
#include "server/server_stats.hpp"

#include <asio.hpp>

#include <array>
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
                                              PubSubBroker*         broker);

    void start();
    ~Connection();  // decrements stats_->connected_clients, unsubscribes from all channels

private:
    Connection(asio::ip::tcp::socket socket,
               KvStore&              store,
               const Dispatcher&     dispatcher,
               AofPersistence*       aof,
               ServerStats*          stats,
               PubSubBroker*         broker);

    void do_read();
    void handle_data(std::size_t bytes);
    void handle_subscribe(const Command& cmd);
    void handle_unsubscribe(const Command& cmd);
    void push_message(std::string_view channel, std::string message);
    void do_write(std::string data);

    asio::ip::tcp::socket        socket_;
    KvStore&                     store_;
    const Dispatcher&            dispatcher_;
    AofPersistence*              aof_;     // non-owning, may be null
    ServerStats*                 stats_;   // non-owning, may be null
    PubSubBroker*                broker_;  // non-owning, may be null
    RespParser                   parser_;
    std::array<char, 4096>       read_buf_;
    std::string                  write_buf_;
    std::unordered_set<std::string> subscribed_channels_;
};

}  // namespace vortek
