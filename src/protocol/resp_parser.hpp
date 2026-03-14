#pragma once

#include "resp_types.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace vortek {

// Incremental RESP2 parser.
//
// Data arrives in arbitrary-sized chunks (e.g. from an async socket read).
// Feed chunks one at a time; the parser accumulates bytes internally and
// returns a completed RespValue as soon as a full message is available.
//
// Usage:
//   RespParser parser;
//   // inside async read loop:
//   if (auto val = parser.feed(chunk)) {
//       handle(*val);
//   }
class RespParser {
public:
    // Feed a chunk of raw bytes.
    // Returns a completed RespValue if one was fully parsed, nullopt otherwise.
    // Throws std::runtime_error on a protocol violation.
    std::optional<RespValue> feed(std::string_view chunk);

    // Reset internal buffer (e.g. after a connection error).
    void reset();

private:
    std::string buf_;

    // All parse_* functions operate on buf_ starting at `offset`.
    // Return value: {parsed_value_or_nullopt, absolute_end_offset_in_buf_}.
    // A nullopt result means more data is needed; end_offset will be 0.
    using ParseResult = std::pair<std::optional<RespValue>, std::size_t>;

    ParseResult parse(std::size_t offset) const;

    ParseResult parse_simple_string(std::size_t offset) const;
    ParseResult parse_error        (std::size_t offset) const;
    ParseResult parse_integer      (std::size_t offset) const;
    ParseResult parse_bulk_string  (std::size_t offset) const;
    ParseResult parse_array        (std::size_t offset) const;

    // Find \r\n at or after `offset`; returns std::string::npos if not found.
    std::size_t find_crlf(std::size_t offset) const;

    // Read the line between `offset` and the next \r\n.
    // Returns {line_content, offset_after_crlf} or {nullopt, 0}.
    std::pair<std::optional<std::string>, std::size_t>
    read_line(std::size_t offset) const;
};

}  // namespace vortek
