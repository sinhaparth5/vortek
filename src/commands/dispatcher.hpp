#pragma once

#include "command.hpp"
#include "core/kv_store.hpp"
#include "protocol/resp_types.hpp"

#include <functional>
#include <string>
#include <unordered_map>

namespace vortek {

using HandlerFn = std::function<RespValue(const Command&, KvStore&)>;

class Dispatcher {
public:
    // Register a handler for a command name (must already be uppercase).
    void register_handler(std::string name, HandlerFn fn);

    // Dispatch a command to its registered handler.
    // Returns an error RespValue if the command is unknown.
    RespValue dispatch(const Command& cmd, KvStore& store) const;

    // Build a Dispatcher pre-loaded with all built-in commands.
    static Dispatcher make_default();

private:
    std::unordered_map<std::string, HandlerFn> handlers_;
};

}  // namespace vortek
