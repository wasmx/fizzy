// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "experimental.hpp"

// Adapted from LLVM.
// https://github.com/llvm/llvm-project/blob/master/llvm/include/llvm/Support/LEB128.h#L128
std::pair<uint64_t, const uint8_t*> decodeULEB128(const uint8_t* p, const uint8_t* end)
{
    uint64_t Value = 0;
    unsigned Shift = 0;
    do
    {
        if (end && p == end)
            throw "malformed uleb128, extends past end";

        uint64_t Slice = *p & 0x7f;
        if (Shift >= 64 || Slice << Shift >> Shift != Slice)
            throw "uleb128 too big for uint64";

        Value += uint64_t(*p & 0x7f) << Shift;
        Shift += 7;
    } while (*p++ >= 128);
    return {Value, p};
}
