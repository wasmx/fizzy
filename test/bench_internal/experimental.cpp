#include "bytes.hpp"
#include <cstdint>
#include <utility>

// Adapted from LLVM.
// https://github.com/llvm/llvm-project/blob/master/llvm/include/llvm/Support/LEB128.h#L80
fizzy::bytes leb128u_encode(uint64_t value)
{
    fizzy::bytes result;
    do
    {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if (value != 0)
            byte |= 0x80;  // Mark this byte to show that more bytes will follow.
        result.push_back(byte);
    } while (value != 0);
    return result;
}

std::pair<uint64_t, const uint8_t*> nop(const uint8_t* p, const uint8_t* end)
{
    auto n = p + 10;
    if (n > end)
        n = end;

    return {*p, n};
}

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
