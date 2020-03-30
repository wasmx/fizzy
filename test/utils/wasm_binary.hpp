#pragma once

#include "leb128_encode.hpp"


namespace fizzy::test
{
/// Creates wasm binary representing i32.const instruction with following encoded immediate value.
inline bytes i32_const(uint32_t c)
{
    return uint8_t{0x41} + leb128u_encode(c);
}
}  // namespace fizzy::test
