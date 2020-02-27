#include "leb128_encode.hpp"

namespace fizzy::test
{
fizzy::bytes leb128u_encode(uint64_t value) noexcept
{
    // Adapted from LLVM.
    // https://github.com/llvm/llvm-project/blob/master/llvm/include/llvm/Support/LEB128.h#L80
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
}  // namespace fizzy::test
