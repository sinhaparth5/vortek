#pragma once

#include "commands/dispatcher.hpp"
#include "server/server_stats.hpp"

namespace vortek::handlers {

// Register PING, DEL, EXISTS, EXPIRE, TTL, PERSIST, INFO into the dispatcher.
// stats is captured by reference for the INFO handler — must outlive the dispatcher.
void register_generic(Dispatcher& dispatcher, ServerStats& stats);

}  // namespace vortek::handlers
