#include "types.hpp"
#include <cstdint>
#include <stdexcept>

namespace fizzy
{
std::pair<uint64_t, const uint8_t*> leb128u_decode(const uint8_t* input)
{
    uint64_t result = 0;
    int result_shift = 0;

    for (; result_shift <= 63; ++input, result_shift += 7)
    {
        // TODO this ignores the bits in the 10th byte other than the least significant one
        // (when result_shift==63)
        // So would not reject some invalid encoding with those with bits set.
        result |= static_cast<uint64_t>(*input & 0x7F) << result_shift;
        if ((*input & 0x80) == 0)
            return {result, input + 1};
    }

    throw std::runtime_error("Invalid unsigned integer encoding.");
}

}  // namespace fizzy