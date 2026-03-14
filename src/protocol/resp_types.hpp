#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace vortek {

// Forward declaration needed for the recursive RespArray type.
// std::vector<T> permits incomplete T in C++17 ([container.requirements.general]/3).
class RespValue;

// ---- Individual RESP2 alternatives ----------------------------------------

struct RespNull {};                                // null bulk string ($-1) or null array (*-1)
struct RespSimpleString { std::string value;   };  // +OK\r\n
struct RespError        { std::string message; };  // -ERR ...\r\n
struct RespInteger      { int64_t     value;   };  // :42\r\n
struct RespBulkString   { std::string value;   };  // $6\r\nfoobar\r\n
struct RespArray        { std::vector<RespValue> elements; };  // *N\r\n...

// ---- Top-level value -------------------------------------------------------

class RespValue {
public:
    using Variant = std::variant<
        RespNull,
        RespSimpleString,
        RespError,
        RespInteger,
        RespBulkString,
        RespArray
    >;

    RespValue() = default;

    // Implicit construction from any alternative (enables ergonomic code).
    template <typename T,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, RespValue>>>
    RespValue(T&& v) : data_(std::forward<T>(v)) {}  // NOLINT(google-explicit-constructor)

    Variant&        data()        { return data_; }
    const Variant&  data()  const { return data_; }

    template <typename T> bool     is()  const { return std::holds_alternative<T>(data_); }
    template <typename T> T&       get()       { return std::get<T>(data_); }
    template <typename T> const T& get() const { return std::get<T>(data_); }

private:
    Variant data_{RespNull{}};
};

}  // namespace vortek
