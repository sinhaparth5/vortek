#pragma once

#include "commands/dispatcher.hpp"

namespace vortek::handlers {

// Register LPUSH, RPUSH, LPOP, RPOP, LLEN, LRANGE, LINDEX into the dispatcher.
void register_list(Dispatcher& dispatcher);

}  // namespace vortek::handlers
