#pragma once

#include "leb128_encode.hpp"


namespace fizzy::test
{
inline bytes add_size_prefix(const bytes& content)
{
    return leb128u_encode(content.size()) + content;
}

inline bytes make_vec(std::initializer_list<bytes_view> contents)
{
    bytes ret = leb128u_encode(contents.size());
    for (const auto& content : contents)
        ret.append(content);
    return ret;
}

inline bytes make_section(uint8_t id, const bytes& content)
{
    return bytes{id} + add_size_prefix(content);
}

/// Creates wasm binary representing i32.const instruction with following encoded immediate value.
inline bytes i32_const(uint32_t c)
{
    return uint8_t{0x41} + leb128u_encode(c);
}
}  // namespace fizzy::test
