#pragma once

#include "resp_types.hpp"

#include <string>

namespace vortek {

// Serialize a RespValue to its RESP2 wire representation.
// Header-only — all functions are inline.

inline std::string serialize(const RespValue& value);  // forward-declared for recursion

inline std::string serialize(const RespNull&) {
    return "$-1\r\n";
}

inline std::string serialize(const RespSimpleString& s) {
    return '+' + s.value + "\r\n";
}

inline std::string serialize(const RespError& e) {
    return '-' + e.message + "\r\n";
}

inline std::string serialize(const RespInteger& i) {
    return ':' + std::to_string(i.value) + "\r\n";
}

inline std::string serialize(const RespBulkString& b) {
    return '$' + std::to_string(b.value.size()) + "\r\n" + b.value + "\r\n";
}

inline std::string serialize(const RespArray& arr) {
    std::string out = '*' + std::to_string(arr.elements.size()) + "\r\n";
    for (const auto& elem : arr.elements)
        out += serialize(elem);
    return out;
}

inline std::string serialize(const RespValue& value) {
    return std::visit([](const auto& v) { return serialize(v); }, value.data());
}

}  // namespace vortek
