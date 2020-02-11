#pragma once

#include "types.hpp"
#include <cstdint>
#include <stdexcept>

namespace fizzy
{
template <typename T>
std::pair<T, const uint8_t*> leb128u_decode(const uint8_t* input)
{
    static_assert(!std::numeric_limits<T>::is_signed);

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

template <typename T>
std::pair<T, const uint8_t*> leb128s_decode(const uint8_t* input)
{
    static_assert(std::numeric_limits<T>::is_signed);

    using T_unsigned = typename std::make_unsigned<T>::type;
    T_unsigned result = 0;
    size_t result_shift = 0;

    for (; result_shift < std::numeric_limits<T_unsigned>::digits; ++input, result_shift += 7)
    {
        result |= static_cast<T_unsigned>((static_cast<T_unsigned>(*input) & 0x7F) << result_shift);
        if ((*input & 0x80) == 0)
        {
            // sign extend
            if ((*input & 0x40) != 0 && (result_shift + 7 < sizeof(T_unsigned) * 8))
            {
                constexpr auto all_ones = std::numeric_limits<T_unsigned>::max();
                const auto mask = static_cast<T_unsigned>(all_ones << (result_shift + 7));
                result |= mask;
            }
            return {static_cast<T>(result), input + 1};
        }
    }

    throw std::runtime_error("Invalid LEB128 encoding: too many bytes.");
}

}  // namespace fizzy