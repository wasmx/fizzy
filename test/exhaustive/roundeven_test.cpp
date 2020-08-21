#include <test/utils/floating_point_utils.hpp>
#include <iostream>

using fizzy::test::FP;


float my_nearest(float x)
{
    const auto u = FP{x}.as_uint();

    if (std::isnan(x))
        return FP{u | 0x400000}.value;

    const auto t = std::trunc(x);

    if (x == t)
        return t;

    const auto is_even = std::fmod(t, 2.0f) == 0;


    if (std::signbit(x) == 0)  // positive
    {
        const auto diff = x - t;

        if (diff < 0.5f)
            return t;

        if (diff > 0.5f)
            return t + 1;



        if (is_even)
            return t;
        else
            return t + 1;
    }
    else
    {
        const auto diff = t - x;

        if (diff < 0.5f)
            return t;

        if (diff > 0.5f)
            return t - 1;

        if (is_even)
            return t;
        else
            return t - 1;
    }
}

int main()
{
    uint32_t i = 0;
    do
    {
        const auto value = FP{i}.value;
        const auto n = my_nearest(value);
        const auto expected = ::roundevenf(value);

        if (FP{n} != FP{expected})
        {
            std::cout << value << " " << n << " " << expected << "\n";
            std::cout << std::hexfloat << value << " " << n << " " << expected << "\n";
            std::cout << std::hex << FP{value}.as_uint() << " " << FP{n}.as_uint() << " "
                      << FP{expected}.as_uint() << "\n";

            std::cout << FP{n}.nan_payload() << " " << FP{expected}.nan_payload();
            return 1;
        }
        ++i;
    } while (i != 0);

    return 0;
}
