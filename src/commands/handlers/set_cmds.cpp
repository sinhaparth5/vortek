#include "set_cmds.hpp"

#include "core/value.hpp"

namespace vortek::handlers {

namespace {

const RespError kWrongType{
    "WRONGTYPE Operation against a key holding the wrong kind of value"};

RespValue wrong_args(std::string_view name) {
    return RespError{"ERR wrong number of arguments for '" + std::string(name) + "' command"};
}

std::optional<SetValue> get_or_create_set(const std::string& key, KvStore& store,
                                          RespValue& out) {
    auto val = store.get(key);
    if (!val) return SetValue{};
    if (!val->is_set()) {
        out = kWrongType;
        return std::nullopt;
    }
    return val->as_set();
}

// SADD key member [member ...]
// → integer: number of members actually added (not already present)
RespValue cmd_sadd(const Command& cmd, KvStore& store) {
    if (cmd.args.size() < 2) return wrong_args(cmd.name);

    RespValue err{RespNull{}};
    auto set_opt = get_or_create_set(cmd.args[0], store, err);
    if (!set_opt) return err;

    auto& s = *set_opt;
    int64_t added = 0;
    for (std::size_t i = 1; i < cmd.args.size(); ++i) {
        if (s.members.insert(cmd.args[i]).second)
            ++added;
    }

    store.set(cmd.args[0], Value{std::move(s)});
    return RespInteger{added};
}

// SREM key member [member ...]
// → integer: number of members actually removed
RespValue cmd_srem(const Command& cmd, KvStore& store) {
    if (cmd.args.size() < 2) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespInteger{0};
    if (!val->is_set()) return kWrongType;

    auto s = val->as_set();
    int64_t removed = 0;
    for (std::size_t i = 1; i < cmd.args.size(); ++i)
        removed += static_cast<int64_t>(s.members.erase(cmd.args[i]));

    if (s.members.empty())
        store.del(cmd.args[0]);
    else
        store.set(cmd.args[0], Value{std::move(s)});

    return RespInteger{removed};
}

// SMEMBERS key → array of all members
RespValue cmd_smembers(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespArray{};
    if (!val->is_set()) return kWrongType;

    RespArray result;
    for (const auto& m : val->as_set().members)
        result.elements.push_back(RespBulkString{m});
    return result;
}

// SISMEMBER key member → 1 if member, 0 otherwise
RespValue cmd_sismember(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 2) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespInteger{0};
    if (!val->is_set()) return kWrongType;

    return RespInteger{val->as_set().members.count(cmd.args[1]) > 0 ? 1 : 0};
}

// SCARD key → integer
RespValue cmd_scard(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespInteger{0};
    if (!val->is_set()) return kWrongType;

    return RespInteger{static_cast<int64_t>(val->as_set().members.size())};
}

}  // namespace

// ---------------------------------------------------------------------------

void register_set(Dispatcher& dispatcher) {
    dispatcher.register_handler("SADD",      cmd_sadd);
    dispatcher.register_handler("SREM",      cmd_srem);
    dispatcher.register_handler("SMEMBERS",  cmd_smembers);
    dispatcher.register_handler("SISMEMBER", cmd_sismember);
    dispatcher.register_handler("SCARD",     cmd_scard);
}

}  // namespace vortek::handlers
