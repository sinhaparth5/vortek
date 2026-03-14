#pragma once

#include "commands/dispatcher.hpp"

namespace vortek::handlers {

// Register PING, DEL, EXISTS, EXPIRE, TTL, PERSIST into the given dispatcher.
void register_generic(Dispatcher& dispatcher);

}  // namespace vortek::handlers
