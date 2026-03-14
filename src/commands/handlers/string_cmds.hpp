#pragma once

#include "commands/dispatcher.hpp"

namespace vortek::handlers {

// Register SET, GET, INCR, INCRBY, DECR, DECRBY into the given dispatcher.
void register_string(Dispatcher& dispatcher);

}  // namespace vortek::handlers
