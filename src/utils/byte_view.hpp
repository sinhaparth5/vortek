#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace vortek {

// Non-owning view over a contiguous sequence of bytes.
using ByteView = std::span<const std::byte>;

// Convenience: wrap a string_view as a ByteView (zero-copy).
inline ByteView as_bytes(std::string_view sv) noexcept {
    return {reinterpret_cast<const std::byte*>(sv.data()), sv.size()};
}

}  // namespace vortek
