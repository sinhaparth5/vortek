#include "dispatcher.hpp"

#include "handlers/generic_cmds.hpp"
#include "handlers/string_cmds.hpp"

namespace vortek {

void Dispatcher::register_handler(std::string name, HandlerFn fn) {
    handlers_.emplace(std::move(name), std::move(fn));
}

RespValue Dispatcher::dispatch(const Command& cmd, KvStore& store) const {
    auto it = handlers_.find(cmd.name);
    if (it == handlers_.end())
        return RespError{"ERR unknown command '" + cmd.name + "'"};
    return it->second(cmd, store);
}

Dispatcher Dispatcher::make_default(ServerStats& stats) {
    Dispatcher d;
    handlers::register_generic(d, stats);
    handlers::register_string(d);
    return d;
}

}  // namespace vortek
