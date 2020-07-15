// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

/// @file
/// Contains generic implementations of the WebAssembly instructions.

#pragma once

#include "stack.hpp"
#include <functional>

namespace fizzy
{
template <typename Op>
inline void unary_op(OperandStack& stack, Op op) noexcept
{
    using T = decltype(op(stack.top()));
    stack.top() = op(static_cast<T>(stack.top()));
}

template <typename Op>
inline void binary_op(OperandStack& stack, Op op) noexcept
{
    using T = decltype(op(stack.top(), stack.top()));
    const auto val2 = static_cast<T>(stack.pop());
    const auto val1 = static_cast<T>(stack.top());
    stack.top() = static_cast<std::make_unsigned_t<T>>(op(val1, val2));
}

template <typename T, template <typename> class Op>
inline void comparison_op(OperandStack& stack, Op<T> op) noexcept
{
    const auto val2 = static_cast<T>(stack.pop());
    const auto val1 = static_cast<T>(stack.top());
    stack.top() = uint32_t{op(val1, val2)};
}

template <typename T>
inline T add(T lhs, T rhs) noexcept
{
    return lhs + rhs;
}

template <typename T>
inline T sub(T lhs, T rhs) noexcept
{
    return lhs - rhs;
}

template <typename T>
inline T mul(T lhs, T rhs) noexcept
{
    return lhs * rhs;
}

template <typename T>
inline T div(T lhs, T rhs) noexcept
{
    return lhs / rhs;
}

template <typename T>
inline T rem(T lhs, T rhs) noexcept
{
    return lhs % rhs;
}

template <typename T>
inline T bit_and(T lhs, T rhs) noexcept
{
    return lhs & rhs;
}

template <typename T>
inline T bit_or(T lhs, T rhs) noexcept
{
    return lhs | rhs;
}

template <typename T>
inline T bit_xor(T lhs, T rhs) noexcept
{
    return lhs ^ rhs;
}

template <typename T>
inline T shift_left(T lhs, T rhs) noexcept
{
    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);
    return lhs << k;
}

template <typename T>
inline T shift_right(T lhs, T rhs) noexcept
{
    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);
    return lhs >> k;
}

template <typename T>
inline T rotl(T lhs, T rhs) noexcept
{
    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);

    if (k == 0)
        return lhs;

    return (lhs << k) | (lhs >> (num_bits - k));
}

template <typename T>
inline T rotr(T lhs, T rhs) noexcept
{
    constexpr T num_bits{sizeof(T) * 8};
    const auto k = rhs & (num_bits - 1);

    if (k == 0)
        return lhs;

    return (lhs >> k) | (lhs << (num_bits - k));
}

inline uint32_t clz32(uint32_t value) noexcept
{
    // NOTE: Wasm specifies this case, but C/C++ intrinsic leaves it as undefined.
    if (value == 0)
        return 32;
    return static_cast<uint32_t>(__builtin_clz(value));
}

inline uint32_t ctz32(uint32_t value) noexcept
{
    // NOTE: Wasm specifies this case, but C/C++ intrinsic leaves it as undefined.
    if (value == 0)
        return 32;
    return static_cast<uint32_t>(__builtin_ctz(value));
}

inline uint32_t popcnt32(uint32_t value) noexcept
{
    return static_cast<uint32_t>(__builtin_popcount(value));
}

inline uint64_t clz64(uint64_t value) noexcept
{
    // NOTE: Wasm specifies this case, but C/C++ intrinsic leaves it as undefined.
    if (value == 0)
        return 64;
    return static_cast<uint64_t>(__builtin_clzll(value));
}

inline uint64_t ctz64(uint64_t value) noexcept
{
    // NOTE: Wasm specifies this case, but C/C++ intrinsic leaves it as undefined.
    if (value == 0)
        return 64;
    return static_cast<uint64_t>(__builtin_ctzll(value));
}

inline uint64_t popcnt64(uint64_t value) noexcept
{
    return static_cast<uint64_t>(__builtin_popcountll(value));
}

}  // namespace fizzy
