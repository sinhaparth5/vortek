#include "pubsub_cmds.hpp"

namespace vortek::handlers {

namespace {

// PUBLISH channel message → integer: number of subscribers that received it
RespValue cmd_publish(const Command& cmd, KvStore& /*store*/, PubSubBroker& broker) {
    if (cmd.args.size() != 2)
        return RespError{"ERR wrong number of arguments for 'publish' command"};

    const int64_t count = broker.publish(cmd.args[0], cmd.args[1]);
    return RespInteger{count};
}

}  // namespace

void register_pubsub(Dispatcher& dispatcher, PubSubBroker& broker) {
    dispatcher.register_handler("PUBLISH",
        [&broker](const Command& cmd, KvStore& store) {
            return cmd_publish(cmd, store, broker);
        });
}

}  // namespace vortek::handlers
