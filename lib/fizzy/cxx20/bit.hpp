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
using std::countl_zero;
using std::countr_zero;
using std::popcount;
}  // namespace fizzy

#else

namespace fizzy
{
inline constexpr int popcount(uint32_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned int));
    return __builtin_popcount(x);
}

inline constexpr int popcount(uint64_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned long long));
    return __builtin_popcountll(x);
}

inline constexpr int countl_zero(uint32_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned int));
    // NOTE: C++20 specifies this case, but the intrinsic leaves it as undefined.
    if (x == 0)
        return 32;
    return __builtin_clz(x);
}

inline constexpr int countl_zero(uint64_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned long long));
    // NOTE: C++20 specifies this case, but the intrinsic leaves it as undefined.
    if (x == 0)
        return 64;
    return __builtin_clzll(x);
}

inline constexpr int countr_zero(uint32_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned int));
    // NOTE: C++20 specifies this case, but the intrinsic leaves it as undefined.
    if (x == 0)
        return 32;
    return __builtin_ctz(x);
}

inline constexpr int countr_zero(uint64_t x) noexcept
{
    static_assert(sizeof(x) == sizeof(unsigned long long));
    // NOTE: C++20 specifies this case, but the intrinsic leaves it as undefined.
    if (x == 0)
        return 64;
    return __builtin_ctzll(x);
}
}  // namespace fizzy

#endif /* __cpp_lib_bitops */
