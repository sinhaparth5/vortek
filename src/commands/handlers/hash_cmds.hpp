#pragma once

#include "commands/dispatcher.hpp"

namespace vortek::handlers {

// Register HSET, HGET, HDEL, HGETALL, HEXISTS, HLEN into the dispatcher.
void register_hash(Dispatcher& dispatcher);

}  // namespace vortek::handlers
