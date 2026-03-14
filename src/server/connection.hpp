#pragma once

#include "commands/dispatcher.hpp"
#include "core/kv_store.hpp"
#include "persistence/aof_persistence.hpp"
#include "protocol/resp_parser.hpp"

#include <asio.hpp>

#include <array>
#include <memory>
#include <string>

namespace vortek {

// Represents a single client connection.
//
// Lifetime is managed via shared_ptr + enable_shared_from_this so that
// outstanding async operations keep the object alive until they complete.
//
// Read/write cycle:
//   do_read() → handle_data() → do_write() → do_read() → ...
class Connection : public std::enable_shared_from_this<Connection> {
public:
    // Factory — private constructor, use this to create connections.
    // aof may be nullptr if persistence is disabled.
    static std::shared_ptr<Connection> create(asio::ip::tcp::socket socket,
                                              KvStore&              store,
                                              const Dispatcher&     dispatcher,
                                              AofPersistence*       aof);

    // Begin the async read/write loop.
    void start();

private:
    Connection(asio::ip::tcp::socket socket,
               KvStore&              store,
               const Dispatcher&     dispatcher,
               AofPersistence*       aof);

    void do_read();
    void handle_data(std::size_t bytes);
    void do_write(std::string data);

    asio::ip::tcp::socket  socket_;
    KvStore&               store_;
    const Dispatcher&      dispatcher_;
    AofPersistence*        aof_;  // non-owning, may be null
    RespParser             parser_;
    std::array<char, 4096> read_buf_;
    std::string            write_buf_;
};

}  // namespace vortek
