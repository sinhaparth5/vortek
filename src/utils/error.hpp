#pragma once

#include <string>

namespace vortek {

enum class ErrorCode {
    Ok = 0,
    WrongType,    // operation against a key holding the wrong kind of value
    OutOfRange,   // integer overflow / underflow
    NotFound,     // key does not exist
    SyntaxError,  // malformed command arguments
    IoError,      // network or file I/O failure
};

struct Error {
    ErrorCode   code    = ErrorCode::Ok;
    std::string message;

    bool ok() const noexcept { return code == ErrorCode::Ok; }

    static Error ok_result() { return {ErrorCode::Ok, {}}; }
};

}  // namespace vortek
