#include "resp_parser.hpp"

#include <charconv>
#include <stdexcept>

namespace vortek {

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::optional<RespValue> RespParser::feed(std::string_view chunk) {
    buf_.append(chunk);

    auto [val, end_pos] = parse(0);
    if (val) {
        buf_.erase(0, end_pos);
        return val;
    }
    return std::nullopt;
}

void RespParser::reset() {
    buf_.clear();
}

std::size_t RespParser::buffered_size() const noexcept {
    return buf_.size();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::size_t RespParser::find_crlf(std::size_t offset) const {
    return buf_.find("\r\n", offset);
}

std::pair<std::optional<std::string>, std::size_t>
RespParser::read_line(std::size_t offset) const {
    std::size_t pos = find_crlf(offset);
    if (pos == std::string::npos)
        return {std::nullopt, 0};
    return {buf_.substr(offset, pos - offset), pos + 2};
}

// ---------------------------------------------------------------------------
// Top-level dispatch
// ---------------------------------------------------------------------------

RespParser::ParseResult RespParser::parse(std::size_t offset) const {
    if (offset >= buf_.size())
        return {std::nullopt, 0};

    switch (buf_[offset]) {
        case '+': return parse_simple_string(offset + 1);
        case '-': return parse_error(offset + 1);
        case ':': return parse_integer(offset + 1);
        case '$': return parse_bulk_string(offset + 1);
        case '*': return parse_array(offset + 1);
        default:
            throw std::runtime_error(
                std::string("RESP: unknown type byte '") + buf_[offset] + '\'');
    }
}

// ---------------------------------------------------------------------------
// Type parsers
// ---------------------------------------------------------------------------

RespParser::ParseResult
RespParser::parse_simple_string(std::size_t offset) const {
    auto [line, end] = read_line(offset);
    if (!line) return {std::nullopt, 0};
    return {RespSimpleString{std::move(*line)}, end};
}

RespParser::ParseResult
RespParser::parse_error(std::size_t offset) const {
    auto [line, end] = read_line(offset);
    if (!line) return {std::nullopt, 0};
    return {RespError{std::move(*line)}, end};
}

RespParser::ParseResult
RespParser::parse_integer(std::size_t offset) const {
    auto [line, end] = read_line(offset);
    if (!line) return {std::nullopt, 0};

    int64_t val{};
    auto [ptr, ec] = std::from_chars(line->data(), line->data() + line->size(), val);
    if (ec != std::errc{} || ptr != line->data() + line->size())
        throw std::runtime_error("RESP: invalid integer '" + *line + '\'');

    return {RespInteger{val}, end};
}

RespParser::ParseResult
RespParser::parse_bulk_string(std::size_t offset) const {
    auto [line, after_len] = read_line(offset);
    if (!line) return {std::nullopt, 0};

    int64_t len{};
    auto [ptr, ec] = std::from_chars(line->data(), line->data() + line->size(), len);
    if (ec != std::errc{} || ptr != line->data() + line->size())
        throw std::runtime_error("RESP: invalid bulk string length '" + *line + '\'');

    if (len == -1)
        return {RespNull{}, after_len};

    if (len < 0)
        throw std::runtime_error("RESP: negative bulk string length");

    auto data_len = static_cast<std::size_t>(len);

    // Need data_len bytes + trailing \r\n
    if (after_len + data_len + 2 > buf_.size())
        return {std::nullopt, 0};

    std::string data = buf_.substr(after_len, data_len);
    return {RespBulkString{std::move(data)}, after_len + data_len + 2};
}

RespParser::ParseResult
RespParser::parse_array(std::size_t offset) const {
    auto [line, after_count] = read_line(offset);
    if (!line) return {std::nullopt, 0};

    int64_t count{};
    auto [ptr, ec] = std::from_chars(line->data(), line->data() + line->size(), count);
    if (ec != std::errc{} || ptr != line->data() + line->size())
        throw std::runtime_error("RESP: invalid array count '" + *line + '\'');

    if (count == -1)
        return {RespNull{}, after_count};

    if (count < 0)
        throw std::runtime_error("RESP: negative array count");

    RespArray arr;
    arr.elements.reserve(static_cast<std::size_t>(count));

    std::size_t cur = after_count;
    for (int64_t i = 0; i < count; ++i) {
        auto [elem, end] = parse(cur);
        if (!elem) return {std::nullopt, 0};
        arr.elements.push_back(std::move(*elem));
        cur = end;
    }

    return {std::move(arr), cur};
}

}  // namespace vortek
