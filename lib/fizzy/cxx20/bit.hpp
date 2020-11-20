// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <type_traits>

#if __has_include(<version>)
#include <version>
#endif

namespace fizzy
{
/// The non-constexpr implementation of C++20's std::bit_cast.
/// See https://en.cppreference.com/w/cpp/numeric/bit_cast.
template <class To, class From>
[[nodiscard]] inline
    typename std::enable_if_t<sizeof(To) == sizeof(From) && std::is_trivially_copyable_v<From> &&
                                  std::is_trivially_copyable_v<To>,
        To>
    bit_cast(const From& src) noexcept
{
    static_assert(std::is_trivially_constructible_v<To>);  // Additional, non-standard requirement.

    To dst;
    __builtin_memcpy(&dst, &src, sizeof(To));
    return dst;
}
}  // namespace fizzy

#ifdef __cpp_lib_bitops

#include <bit>

namespace fizzy
{
constexpr int popcount(uint32_t x) noexcept
{
    return std::popcount(x);
}

constexpr int popcount(uint64_t x) noexcept
{
    return std::popcount(x);
}

constexpr int countl_zero(uint32_t value) noexcept
{
    return std::countl_zero(value);
}

constexpr int countl_zero(uint64_t value) noexcept
{
    return std::countl_zero(value);
}

constexpr int countr_zero(uint32_t value) noexcept
{
    return std::countr_zero(value);
}

constexpr int countr_zero(uint64_t value) noexcept
{
    return std::countr_zero(value);
}

}  // namespace fizzy

#else

namespace fizzy
{
constexpr int popcount(uint32_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned int));
    return __builtin_popcount(x);
}

constexpr int popcount(uint64_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned long long));
    return __builtin_popcountll(x);
}

constexpr int countl_zero(uint32_t value) noexcept
{
    if (value == 0)
        return 32;
    return __builtin_clz(value);
}

constexpr int countl_zero(uint64_t value) noexcept
{
    if (value == 0)
        return 64;
    return __builtin_clzll(value);
}

constexpr int countr_zero(uint32_t value) noexcept
{
    if (value == 0)
        return 32;
    return __builtin_ctz(value);
}

constexpr int countr_zero(uint64_t value) noexcept
{
    if (value == 0)
        return 64;
    return __builtin_ctzll(value);
}
}  // namespace fizzy

#endif /* __cpp_lib_bitops */
