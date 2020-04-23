// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "leb128.hpp"

std::pair<uint64_t, const uint8_t*> leb128u_decode_u64_noinline(
    const uint8_t* input, const uint8_t* end)
{
    return fizzy::leb128u_decode<uint64_t>(input, end);
}
