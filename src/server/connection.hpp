#pragma once

#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "persistence/aof_persistence.hpp"
#include "protocol/resp_parser.hpp"
#include "server/server_stats.hpp"

#include <asio.hpp>

#include <array>
#include <memory>
#include <string>

namespace vortek {

class Connection : public std::enable_shared_from_this<Connection> {
public:
    // aof and stats may be nullptr.
    static std::shared_ptr<Connection> create(asio::ip::tcp::socket socket,
                                              KvStore&              store,
                                              const Dispatcher&     dispatcher,
                                              AofPersistence*       aof,
                                              ServerStats*          stats);

    void start();
    ~Connection();  // decrements stats_->connected_clients

private:
    Connection(asio::ip::tcp::socket socket,
               KvStore&              store,
               const Dispatcher&     dispatcher,
               AofPersistence*       aof,
               ServerStats*          stats);

    void do_read();
    void handle_data(std::size_t bytes);
    void do_write(std::string data);

    asio::ip::tcp::socket  socket_;
    KvStore&               store_;
    const Dispatcher&      dispatcher_;
    AofPersistence*        aof_;    // non-owning, may be null
    ServerStats*           stats_;  // non-owning, may be null
    RespParser             parser_;
    std::array<char, 4096> read_buf_;
    std::string            write_buf_;
};

}  // namespace vortek
