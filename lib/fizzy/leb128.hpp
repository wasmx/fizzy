#include "types.hpp"
#include <cstdint>
#include <stdexcept>

namespace fizzy
{
uint64_t leb128u_decode(bytes_view input)
{
    uint64_t result = 0;
    int result_shift = 0;

    for (auto byte = input.cbegin(); byte != input.cend() && result_shift <= 63;
         ++byte, result_shift += 7)
    {
        // TODO this ignores the bits in the 10th byte other than the least significant one
        // (when result_shift==63)
        // So would not reject some invalid encoding with those with bits set.
        result |= static_cast<uint64_t>(*byte & 0x7F) << result_shift;
        if ((*byte & 0x80) == 0)
            return result;
    }

    throw std::runtime_error("Invalid unsigned integer encoding.");
}
}  // namespace fizzy