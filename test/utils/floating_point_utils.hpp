// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fizzy::test
{
/// Simple implementation of C++20's std::bit_cast.
template <typename DstT, typename SrcT>
DstT bit_cast(SrcT x) noexcept
{
    DstT z;
    static_assert(sizeof(x) == sizeof(z));
    __builtin_memcpy(&z, &x, sizeof(x));
    return z;
}

/// A wrapper for floating-point types with inspection/construction/comparison utilities.
template <typename T>
struct FP
{
    static_assert(std::is_same_v<T, float> || std::is_same_v<T, double>);

    /// Shortcut to numeric_limits.
    using Limits = std::numeric_limits<T>;
    static_assert(Limits::is_iec559);

    /// The unsigned integer type matching the size of this floating-point type.
    using UintType = std::conditional_t<std::is_same_v<T, float>, uint32_t, uint64_t>;
    static_assert(sizeof(T) == sizeof(UintType));

    /// The number of mantissa bits in the binary representation.
    static constexpr auto num_mantissa_bits = Limits::digits - 1;

    /// The binary mask of the mantissa part of the binary representation.
    static constexpr auto mantissa_mask = (UintType{1} << num_mantissa_bits) - 1;

    /// The number of exponent bits in the binary representation.
    static constexpr auto num_exponent_bits = int{sizeof(T) * 8} - num_mantissa_bits - 1;

    /// The exponent value (all exponent bits set) for NaNs.
    static constexpr auto nan_exponent = (UintType{1} << num_exponent_bits) - 1;

    /// The payload of the canonical NaN (only the top bit set).
    /// See: https://webassembly.github.io/spec/core/syntax/values.html#canonical-nan.
    static constexpr auto canon = UintType{1} << (num_mantissa_bits - 1);

    T value{};

    explicit FP(T v) noexcept : value{v} {};

    explicit FP(UintType u) noexcept : value{bit_cast<T>(u)} {};

    /// Return unsigned integer with the binary representation of the value.
    UintType as_uint() const noexcept { return bit_cast<UintType>(value); }

    /// Returns NaN payload if the value is a NaN, otherwise 0 (NaN payload is never 0).
    UintType nan_payload() const noexcept
    {
        return std::isnan(value) ? (as_uint() & mantissa_mask) : 0;
    }

    bool is_canonical_nan() const noexcept { return nan_payload() == canon; }

    bool is_arithmetic_nan() const noexcept { return nan_payload() >= canon; }

    /// Build the NaN value with the given payload.
    ///
    /// The NaN values have any sign, all exponent bits set, and non-zero mantissa (otherwise they
    /// would be infinities).
    /// The IEEE 754 defines quiet NaN as having the top bit of the mantissa set to 1. Wasm calls
    /// this NaN _arithmetic_. The arithmetic NaN with the lowest mantissa (the top bit set, all
    /// other zeros) is the _canonical_ NaN.
    static T nan(UintType payload) noexcept
    {
        return FP{(nan_exponent << num_mantissa_bits) | (payload & mantissa_mask)}.value;
    }

    friend bool operator==(FP a, FP b) noexcept { return a.as_uint() == b.as_uint(); }
    friend bool operator==(FP a, T b) noexcept { return a == FP{b}; }
    friend bool operator==(T a, FP b) noexcept { return FP{a} == b; }

    friend bool operator!=(FP a, FP b) noexcept { return !(a == b); }
    friend bool operator!=(FP a, T b) noexcept { return a != FP{b}; }
    friend bool operator!=(T a, FP b) noexcept { return FP{a} != b; }
};

FP(uint32_t)->FP<float>;
FP(uint64_t)->FP<double>;

using FP32 = FP<float>;
using FP64 = FP<double>;
}  // namespace fizzy::test
