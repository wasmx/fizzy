#pragma once

#include "bytes.hpp"
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

inline namespace literals
{
/// Operator for "" literals, e.g. "0a0b0c0d"_bytes.
inline fizzy::bytes operator""_bytes(const char* literal, size_t /*length*/)
{
    return fizzy::from_hex(literal);
}
}  // namespace literals
}  // namespace fizzy
