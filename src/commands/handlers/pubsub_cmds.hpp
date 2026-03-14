#pragma once

#include "commands/dispatcher.hpp"
#include "server/pubsub_broker.hpp"

namespace vortek::handlers {

// Register PUBLISH into the dispatcher.
// broker must outlive the dispatcher.
void register_pubsub(Dispatcher& dispatcher, PubSubBroker& broker);

}  // namespace vortek::handlers
