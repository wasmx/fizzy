#include <test/utils/floating_point_utils.hpp>
#include <cfenv>
#include <chrono>
#include <iostream>

using fizzy::test::FP;

template <typename T>
static bool is_even(T x)
{
    using FP = FP<T>;
    const auto u = FP{x}.as_uint();

    const auto m = u & FP::mantissa_mask;

    constexpr auto exponent_mask = (typename FP::UintType{1} << FP::num_exponent_bits) - 1;
    constexpr auto exponent_bias = (typename FP::UintType{1} << (FP::num_exponent_bits - 1)) - 1;

    const auto e = ((u >> FP::num_mantissa_bits) & exponent_mask) - exponent_bias;

    if (e == 0)  // 1
        return false;

    const auto lowest_bit = FP::num_mantissa_bits - e;

    return ((m >> lowest_bit) & 1) == 0;
}


float my_nearest(float x)
{
    if (std::isnan(x))
        return FP{FP{x}.as_uint() | 0x400000}.value;

    auto t = std::trunc(x);

    const auto diff = std::abs(x - t);

    if (diff > 0.5f || (diff == 0.5f && !is_even(t)))
        t = t + std::copysign(1.0f, x);

    return t;
}


float roundeven_simple(float x)
{
    static constexpr auto is_even = [](auto i) noexcept { return std::fmod(i, 2.0f) == 0; };

    const auto t = std::trunc(x);
    if (const auto diff = std::abs(x - t); diff > 0.5f || (diff == 0.5f && !is_even(t)))
        return t + std::copysign(1.0f, x);
    else
        return t;
}

int main()
{
    using clock = std::chrono::steady_clock;
    const auto start_time = clock::now();

    for (const auto rounding_direction : {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO})
    {
        std::fesetround(rounding_direction);

        uint32_t i = 0;
        do
        {
            const auto value = FP{i}.value;
            const auto n = roundeven_simple(value);
            const auto expected = ::roundevenf(value);

            if (FP{n} != FP{expected})
            {
                if (!std::isnan(n) || !std::isnan(expected))
                {
                    std::cout << value << " " << n << " " << expected << "\n";
                    std::cout << std::hexfloat << value << " " << n << " " << expected << "\n";
                    std::cout << std::hex << FP{value}.as_uint() << " " << FP{n}.as_uint() << " "
                              << FP{expected}.as_uint() << "\n";

                    std::cout << FP{n}.nan_payload() << " " << FP{expected}.nan_payload();
                    return 1;
                }
            }
            ++i;
        } while (i != 0);
    }

    std::cout
        << "time: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() - start_time).count()
        << " ms\n";
    return 0;
}
