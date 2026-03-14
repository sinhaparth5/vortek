#pragma once

#include <string>
#include <variant>

namespace vortek {

// Each concrete type is a named struct so the variant stays extensible.
// Adding a new type (e.g. ListValue) never breaks existing code.

struct StringValue { std::string data; };

// Reserved for future phases:
// struct ListValue   { ... };
// struct SetValue    { ... };
// struct HashValue   { ... };

using ValueData = std::variant<StringValue>;

class Value {
public:
    explicit Value(std::string s) : data_(StringValue{std::move(s)}) {}

    bool        is_string()  const { return std::holds_alternative<StringValue>(data_); }
    std::string&       as_string()       { return std::get<StringValue>(data_).data; }
    const std::string& as_string() const { return std::get<StringValue>(data_).data; }

    ValueData&       data()       { return data_; }
    const ValueData& data() const { return data_; }

private:
    ValueData data_;
};

}  // namespace vortek
