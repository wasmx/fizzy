// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "cxx20/bit.hpp"
#include <cmath>
#include <cstdint>
#include <limits>
#include <ostream>
#include <type_traits>

#ifdef __i386__
// Signaling NaNs are not fully supported on x87.
#define SNAN_SUPPORTED 0
#else
#define SNAN_SUPPORTED 1
#endif

namespace fizzy::test
{
/// A wrapper for floating-point types with inspection/construction/comparison utilities.
template <typename T>
struct FP
{
    /// The wrapped floating-point type.
    using FloatType = T;

    static_assert(std::is_same_v<FloatType, float> || std::is_same_v<FloatType, double>);

    /// Shortcut to numeric_limits.
    using Limits = std::numeric_limits<FloatType>;
    static_assert(Limits::is_iec559);

    /// The unsigned integer type matching the size of this floating-point type.
    using UintType = std::conditional_t<std::is_same_v<FloatType, float>, uint32_t, uint64_t>;
    static_assert(sizeof(FloatType) == sizeof(UintType));

    /// The number of mantissa bits in the binary representation.
    static constexpr auto num_mantissa_bits = Limits::digits - 1;

    /// The binary mask of the mantissa part of the binary representation.
    static constexpr auto mantissa_mask = (UintType{1} << num_mantissa_bits) - 1;

    /// The number of exponent bits in the binary representation.
    static constexpr auto num_exponent_bits = int{sizeof(FloatType) * 8} - num_mantissa_bits - 1;

    /// The exponent value (all exponent bits set) for NaNs.
    static constexpr auto nan_exponent = (UintType{1} << num_exponent_bits) - 1;

    /// The payload of the canonical NaN (only the top bit set).
    /// See: https://webassembly.github.io/spec/core/syntax/values.html#canonical-nan.
    static constexpr auto canon = UintType{1} << (num_mantissa_bits - 1);

private:
    UintType m_storage{};  ///< Bits storage.

public:
    explicit FP(FloatType v) noexcept : m_storage{bit_cast<UintType>(v)} {}

    explicit FP(UintType u) noexcept : m_storage{u} {}

    /// Return unsigned integer with the binary representation of the value.
    UintType as_uint() const noexcept { return m_storage; }

    /// Return the floating-point value.
    FloatType as_float() const noexcept { return bit_cast<FloatType>(m_storage); }

    /// Returns true if the value is a NaN.
    ///
    /// The implementation only inspects the bit patterns in the storage.
    /// Using floating-point functions like std::isnan() is explicitly avoided because
    /// passing/returning float value to/from functions causes signaling-NaN to quiet-NaN
    /// conversions on some architectures (e.g. i368).
    bool is_nan() const noexcept
    {
        const auto exponent = (m_storage >> num_mantissa_bits) & nan_exponent;
        const auto mantissa = m_storage & mantissa_mask;
        return exponent == nan_exponent && mantissa != 0;
    }

    /// Returns NaN payload if the value is a NaN, otherwise 0 (NaN payload is never 0).
    UintType nan_payload() const noexcept { return is_nan() ? (as_uint() & mantissa_mask) : 0; }

    bool is_canonical_nan() const noexcept { return nan_payload() == canon; }

    bool is_arithmetic_nan() const noexcept { return nan_payload() >= canon; }

    /// Build the NaN value with the given payload.
    ///
    /// The NaN values have any sign, all exponent bits set, and non-zero mantissa (otherwise they
    /// would be infinities).
    /// The IEEE 754 defines quiet NaN as having the top bit of the mantissa set to 1. Wasm calls
    /// this NaN _arithmetic_. The arithmetic NaN with the lowest mantissa (the top bit set, all
    /// other zeros) is the _canonical_ NaN.
    static FloatType nan(UintType payload) noexcept
    {
        return FP{(nan_exponent << num_mantissa_bits) | (payload & mantissa_mask)}.as_float();
    }

    friend bool operator==(FP a, FP b) noexcept { return a.as_uint() == b.as_uint(); }
    friend bool operator==(FP a, FloatType b) noexcept { return a == FP{b}; }
    friend bool operator==(FloatType a, FP b) noexcept { return FP{a} == b; }

    friend bool operator!=(FP a, FP b) noexcept { return !(a == b); }
    friend bool operator!=(FP a, FloatType b) noexcept { return a != FP{b}; }
    friend bool operator!=(FloatType a, FP b) noexcept { return FP{a} != b; }

    friend std::ostream& operator<<(std::ostream& os, FP x)
    {
        const auto format_flags = os.flags();
        os << x.as_float() << " [";
        if (x.is_nan())
            os << std::hex << x.nan_payload();
        else
            os << std::hexfloat << x.as_float();
        os << "]";
        os.flags(format_flags);
        return os;
    }
};

FP(uint32_t)->FP<float>;
FP(uint64_t)->FP<double>;

using FP32 = FP<float>;
using FP64 = FP<double>;
}  // namespace fizzy::test
