#pragma once

#include "commands/dispatcher.hpp"

namespace vortek::handlers {

// Register SADD, SREM, SMEMBERS, SISMEMBER, SCARD into the dispatcher.
void register_set(Dispatcher& dispatcher);

}  // namespace vortek::handlers
