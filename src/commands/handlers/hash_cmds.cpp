#include "hash_cmds.hpp"

#include "core/value.hpp"

namespace vortek::handlers {

namespace {

const RespError kWrongType{
    "WRONGTYPE Operation against a key holding the wrong kind of value"};

RespValue wrong_args(std::string_view name) {
    return RespError{"ERR wrong number of arguments for '" + std::string(name) + "' command"};
}

std::optional<HashValue> get_or_create_hash(const std::string& key, KvStore& store,
                                             RespValue& out) {
    auto val = store.get(key);
    if (!val) return HashValue{};
    if (!val->is_hash()) {
        out = kWrongType;
        return std::nullopt;
    }
    return val->as_hash();
}

// HSET key field value [field value ...]
// → integer: number of fields newly added (not updated)
RespValue cmd_hset(const Command& cmd, KvStore& store) {
    // Need key + at least one field/value pair (even number of remaining args)
    if (cmd.args.size() < 3 || (cmd.args.size() % 2) == 0)
        return wrong_args(cmd.name);

    RespValue err{RespNull{}};
    auto hash_opt = get_or_create_hash(cmd.args[0], store, err);
    if (!hash_opt) return err;

    auto& h = *hash_opt;
    int64_t added = 0;
    for (std::size_t i = 1; i < cmd.args.size(); i += 2) {
        auto [it, inserted] = h.fields.insert_or_assign(cmd.args[i], cmd.args[i + 1]);
        if (inserted) ++added;
    }

    store.set(cmd.args[0], Value{std::move(h)});
    return RespInteger{added};
}

// HGET key field → bulk string or null
RespValue cmd_hget(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 2) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespNull{};
    if (!val->is_hash()) return kWrongType;

    auto it = val->as_hash().fields.find(cmd.args[1]);
    if (it == val->as_hash().fields.end()) return RespNull{};
    return RespBulkString{it->second};
}

// HDEL key field [field ...] → integer: number of fields removed
RespValue cmd_hdel(const Command& cmd, KvStore& store) {
    if (cmd.args.size() < 2) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespInteger{0};
    if (!val->is_hash()) return kWrongType;

    auto h = val->as_hash();
    int64_t removed = 0;
    for (std::size_t i = 1; i < cmd.args.size(); ++i)
        removed += static_cast<int64_t>(h.fields.erase(cmd.args[i]));

    if (h.fields.empty())
        store.del(cmd.args[0]);
    else
        store.set(cmd.args[0], Value{std::move(h)});

    return RespInteger{removed};
}

// HGETALL key → flat array [field, value, field, value, ...]
RespValue cmd_hgetall(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespArray{};
    if (!val->is_hash()) return kWrongType;

    RespArray result;
    for (const auto& [field, value] : val->as_hash().fields) {
        result.elements.push_back(RespBulkString{field});
        result.elements.push_back(RespBulkString{value});
    }
    return result;
}

// HEXISTS key field → 1 or 0
RespValue cmd_hexists(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 2) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespInteger{0};
    if (!val->is_hash()) return kWrongType;

    return RespInteger{val->as_hash().fields.count(cmd.args[1]) > 0 ? 1 : 0};
}

// HLEN key → integer
RespValue cmd_hlen(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespInteger{0};
    if (!val->is_hash()) return kWrongType;

    return RespInteger{static_cast<int64_t>(val->as_hash().fields.size())};
}

}  // namespace

// ---------------------------------------------------------------------------

void register_hash(Dispatcher& dispatcher) {
    dispatcher.register_handler("HSET",    cmd_hset);
    dispatcher.register_handler("HGET",    cmd_hget);
    dispatcher.register_handler("HDEL",    cmd_hdel);
    dispatcher.register_handler("HGETALL", cmd_hgetall);
    dispatcher.register_handler("HEXISTS", cmd_hexists);
    dispatcher.register_handler("HLEN",    cmd_hlen);
}

}  // namespace vortek::handlers
