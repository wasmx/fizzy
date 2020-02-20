#include "leb128.hpp"

std::pair<uint64_t, const uint8_t*> leb128u_decode_u64_noinline(
    const uint8_t* input, const uint8_t* end)
{
    return fizzy::leb128u_decode<uint64_t>(input, end);
}
