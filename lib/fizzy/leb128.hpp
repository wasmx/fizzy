#pragma once

#include "types.hpp"
#include <cstdint>
#include <stdexcept>

namespace fizzy
{
template <typename T>
std::pair<T, const uint8_t*> leb128u_decode(const uint8_t* input)
{
    T result = 0;
    int result_shift = 0;

    for (; result_shift < std::numeric_limits<T>::digits; ++input, result_shift += 7)
    {
        // TODO this ignores the bits in the last byte other than the least significant one
        // So would not reject some invalid encoding with those bits set.
        result |= static_cast<T>((static_cast<T>(*input) & 0x7F) << result_shift);
        if ((*input & 0x80) == 0)
            return {result, input + 1};
    }

    throw std::runtime_error("Invalid LEB128 encoding: too many bytes.");
}

}  // namespace fizzy