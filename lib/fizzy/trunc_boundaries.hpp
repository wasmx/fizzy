// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace fizzy
{
/// Provides _exclusive_ lower and upper boundaries of the range
/// for which SrcT -> DstT trunc operation has defined results.
/// Theoretical values are (INTEGER_MIN - 1, INTEGER_MAX + 1) which is
/// (-2^N - 1, 2^N) where N is the number of integer bits without the sign bit.
/// However, not all of the theoretical values are representable by the given
/// floating-point type. Deviations are explained below.
template <typename SrcT, typename DstT>
struct trunc_boundaries;

template <>
struct trunc_boundaries<float, int32_t>
{
    /// The first representable value lower than theoretical -2147483649.
    static constexpr float lower = -2147483904.0;

    static constexpr float upper = 2147483648.0;
};

template <>
struct trunc_boundaries<float, uint32_t>
{
    static constexpr float lower = -1.0;
    static constexpr float upper = 4294967296.0;
};

template <>
struct trunc_boundaries<double, int32_t>
{
    static constexpr double lower = -2147483649.0;
    static constexpr double upper = 2147483648.0;
};

template <>
struct trunc_boundaries<double, uint32_t>
{
    static constexpr double lower = -1.0;
    static constexpr double upper = 4294967296.0;
};

template <>
struct trunc_boundaries<float, int64_t>
{
    /// The first representable value lower than theoretical -9223372036854775809.
    static constexpr float lower = -9223373136366403584.0;

    static constexpr float upper = 9223372036854775808.0;
};

template <>
struct trunc_boundaries<float, uint64_t>
{
    static constexpr float lower = -1.0;
    static constexpr float upper = 18446744073709551616.0;
};

template <>
struct trunc_boundaries<double, int64_t>
{
    /// The first representable value lower than theoretical -9223372036854775809.
    static constexpr double lower = -9223372036854777856.0;

    static constexpr double upper = 9223372036854775808.0;
};

template <>
struct trunc_boundaries<double, uint64_t>
{
    static constexpr double lower = -1.0;
    static constexpr double upper = 18446744073709551616.0;
};
}  // namespace fizzy
