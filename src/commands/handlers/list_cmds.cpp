#include "list_cmds.hpp"

#include "core/value.hpp"

#include <charconv>

namespace vortek::handlers {

namespace {

const RespError kWrongType{
    "WRONGTYPE Operation against a key holding the wrong kind of value"};

RespValue wrong_args(std::string_view name) {
    return RespError{"ERR wrong number of arguments for '" + std::string(name) + "' command"};
}

// Retrieve an existing ListValue or create an empty one.
// Returns nullopt (and an error in `out`) if the key holds a different type.
std::optional<ListValue> get_or_create_list(const std::string& key, KvStore& store,
                                            RespValue& out) {
    auto val = store.get(key);
    if (!val) return ListValue{};
    if (!val->is_list()) {
        out = kWrongType;
        return std::nullopt;
    }
    return val->as_list();
}

// LPUSH key element [element ...]
// Prepends elements left-to-right so the last given element ends up at head.
RespValue cmd_lpush(const Command& cmd, KvStore& store) {
    if (cmd.args.size() < 2) return wrong_args(cmd.name);

    RespValue err{RespNull{}};
    auto list_opt = get_or_create_list(cmd.args[0], store, err);
    if (!list_opt) return err;

    auto& list = *list_opt;
    for (std::size_t i = 1; i < cmd.args.size(); ++i)
        list.elements.push_front(cmd.args[i]);

    const auto len = static_cast<int64_t>(list.elements.size());
    store.set(cmd.args[0], Value{std::move(list)});
    return RespInteger{len};
}

// RPUSH key element [element ...]
RespValue cmd_rpush(const Command& cmd, KvStore& store) {
    if (cmd.args.size() < 2) return wrong_args(cmd.name);

    RespValue err{RespNull{}};
    auto list_opt = get_or_create_list(cmd.args[0], store, err);
    if (!list_opt) return err;

    auto& list = *list_opt;
    for (std::size_t i = 1; i < cmd.args.size(); ++i)
        list.elements.push_back(cmd.args[i]);

    const auto len = static_cast<int64_t>(list.elements.size());
    store.set(cmd.args[0], Value{std::move(list)});
    return RespInteger{len};
}

// LPOP key
RespValue cmd_lpop(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespNull{};
    if (!val->is_list()) return kWrongType;

    auto list = val->as_list();
    if (list.elements.empty()) return RespNull{};

    std::string elem = std::move(list.elements.front());
    list.elements.pop_front();

    if (list.elements.empty())
        store.del(cmd.args[0]);
    else
        store.set(cmd.args[0], Value{std::move(list)});

    return RespBulkString{std::move(elem)};
}

// RPOP key
RespValue cmd_rpop(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespNull{};
    if (!val->is_list()) return kWrongType;

    auto list = val->as_list();
    if (list.elements.empty()) return RespNull{};

    std::string elem = std::move(list.elements.back());
    list.elements.pop_back();

    if (list.elements.empty())
        store.del(cmd.args[0]);
    else
        store.set(cmd.args[0], Value{std::move(list)});

    return RespBulkString{std::move(elem)};
}

// LLEN key
RespValue cmd_llen(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 1) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespInteger{0};
    if (!val->is_list()) return kWrongType;

    return RespInteger{static_cast<int64_t>(val->as_list().elements.size())};
}

// LRANGE key start stop  (inclusive, supports negative indices)
RespValue cmd_lrange(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 3) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespArray{};
    if (!val->is_list()) return kWrongType;

    const auto& elems = val->as_list().elements;
    const auto  n     = static_cast<int64_t>(elems.size());

    auto parse_idx = [](const std::string& s, int64_t& out) -> bool {
        auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
        return ec == std::errc{} && ptr == s.data() + s.size();
    };

    int64_t start{}, stop{};
    if (!parse_idx(cmd.args[1], start) || !parse_idx(cmd.args[2], stop))
        return RespError{"ERR value is not an integer or out of range"};

    // Normalise negative indices
    if (start < 0) start += n;
    if (stop  < 0) stop  += n;
    if (start < 0) start = 0;
    if (stop  >= n) stop  = n - 1;

    RespArray result;
    if (start <= stop) {
        for (auto i = start; i <= stop; ++i)
            result.elements.push_back(RespBulkString{elems[static_cast<std::size_t>(i)]});
    }
    return result;
}

// LINDEX key index
RespValue cmd_lindex(const Command& cmd, KvStore& store) {
    if (cmd.args.size() != 2) return wrong_args(cmd.name);

    auto val = store.get(cmd.args[0]);
    if (!val) return RespNull{};
    if (!val->is_list()) return kWrongType;

    const auto& elems = val->as_list().elements;
    const auto  n     = static_cast<int64_t>(elems.size());

    int64_t idx{};
    auto [ptr, ec] = std::from_chars(cmd.args[1].data(),
                                     cmd.args[1].data() + cmd.args[1].size(), idx);
    if (ec != std::errc{} || ptr != cmd.args[1].data() + cmd.args[1].size())
        return RespError{"ERR value is not an integer or out of range"};

    if (idx < 0) idx += n;
    if (idx < 0 || idx >= n) return RespNull{};

    return RespBulkString{elems[static_cast<std::size_t>(idx)]};
}

}  // namespace

// ---------------------------------------------------------------------------

void register_list(Dispatcher& dispatcher) {
    dispatcher.register_handler("LPUSH",  cmd_lpush);
    dispatcher.register_handler("RPUSH",  cmd_rpush);
    dispatcher.register_handler("LPOP",   cmd_lpop);
    dispatcher.register_handler("RPOP",   cmd_rpop);
    dispatcher.register_handler("LLEN",   cmd_llen);
    dispatcher.register_handler("LRANGE", cmd_lrange);
    dispatcher.register_handler("LINDEX", cmd_lindex);
}

}  // namespace vortek::handlers
