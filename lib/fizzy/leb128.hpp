#include <cstdint>

namespace fizzy
{
inline constexpr uint64_t leb128u_decode(const uint8_t* input) noexcept
{
    uint64_t result = 0;
    uint8_t result_shift = 0;

    for (auto byte = input;; ++byte, result_shift += 7)
    {
        result |= static_cast<uint64_t>(*byte & 0x7F) << result_shift;
        if ((*byte & 0x80) == 0)
            break;
    }

    return result;
}
}  // namespace fizzy