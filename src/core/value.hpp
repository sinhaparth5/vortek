#pragma once

#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace vortek {

// Each concrete type is a named struct so the variant stays extensible.
struct StringValue { std::string data; };
struct ListValue   { std::deque<std::string> elements; };
struct SetValue    { std::unordered_set<std::string> members; };
struct HashValue   { std::unordered_map<std::string, std::string> fields; };

using ValueData = std::variant<StringValue, ListValue, SetValue, HashValue>;

class Value {
public:
    explicit Value(std::string s)  : data_(StringValue{std::move(s)}) {}
    explicit Value(ListValue l)    : data_(std::move(l)) {}
    explicit Value(SetValue s)     : data_(std::move(s)) {}
    explicit Value(HashValue h)    : data_(std::move(h)) {}

    bool is_string() const { return std::holds_alternative<StringValue>(data_); }
    bool is_list()   const { return std::holds_alternative<ListValue>(data_); }
    bool is_set()    const { return std::holds_alternative<SetValue>(data_); }
    bool is_hash()   const { return std::holds_alternative<HashValue>(data_); }

    std::string&       as_string()       { return std::get<StringValue>(data_).data; }
    const std::string& as_string() const { return std::get<StringValue>(data_).data; }

    ListValue&       as_list()       { return std::get<ListValue>(data_); }
    const ListValue& as_list() const { return std::get<ListValue>(data_); }

    SetValue&       as_set()       { return std::get<SetValue>(data_); }
    const SetValue& as_set() const { return std::get<SetValue>(data_); }

    HashValue&       as_hash()       { return std::get<HashValue>(data_); }
    const HashValue& as_hash() const { return std::get<HashValue>(data_); }

    ValueData&       data()       { return data_; }
    const ValueData& data() const { return data_; }

private:
    ValueData data_;
};

}  // namespace vortek
