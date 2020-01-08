#pragma once

#include "types.hpp"
#include <cstdint>

namespace fizzy
{
/// Encode a byte to a hex string.
inline std::string hex(uint8_t b) noexcept
{
    static constexpr auto hex_chars = "0123456789abcdef";
    return {hex_chars[b >> 4], hex_chars[b & 0xf]};
}

/// Decodes hex encoded string to bytes.
///
/// Exceptions:
/// - std::length_error when the input has invalid length (must be even).
/// - std::out_of_range when invalid hex digit encountered.
bytes from_hex(const std::string& hex);

/// Encodes bytes as hex string.
std::string hex(const uint8_t* data, size_t size);

/// Encodes bytes as hex string.
inline std::string hex(bytes_view data)
{
    return hex(data.data(), data.size());
}
}  // namespace fizzy
