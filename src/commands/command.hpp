#pragma once

#include "protocol/resp_types.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

namespace vortek {

// A parsed client command: name (always uppercase) + the remaining arguments.
// Clients send commands as RESP arrays, e.g.:
//   *3\r\n$3\r\nSET\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
struct Command {
    std::string              name;  // e.g. "SET", "GET", "PING"
    std::vector<std::string> args;  // everything after the command name
};

// Parse a Command from a client-sent RESP value (must be a non-empty array of
// bulk strings). Returns nullopt if the value is malformed.
inline std::optional<Command> parse_command(const RespValue& val) {
    if (!val.is<RespArray>()) return std::nullopt;

    const auto& elems = val.get<RespArray>().elements;
    if (elems.empty()) return std::nullopt;

    // First element is the command name — must be a bulk string.
    if (!elems[0].is<RespBulkString>()) return std::nullopt;

    Command cmd;
    cmd.name = elems[0].get<RespBulkString>().value;

    // Normalise to uppercase so handlers can compare case-insensitively.
    std::transform(cmd.name.begin(), cmd.name.end(), cmd.name.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });

    // Remaining elements become the argument list.
    for (std::size_t i = 1; i < elems.size(); ++i) {
        if (!elems[i].is<RespBulkString>()) return std::nullopt;
        cmd.args.push_back(elems[i].get<RespBulkString>().value);
    }

    return cmd;
}

}  // namespace vortek
