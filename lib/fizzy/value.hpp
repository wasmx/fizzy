// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cassert>
#include <cstdint>
#include <limits>

namespace fizzy
{
union Value
{
    uint64_t i64;
    float f32;
    double f64;

    static_assert(std::numeric_limits<decltype(f32)>::is_iec559);
    static_assert(std::numeric_limits<decltype(f64)>::is_iec559);

    Value() = default;

    /// Converting constructors from integer types
    ///
    /// We need to support {signed,unsigned} x {32,64} integers. However, due to uint64_t being
    /// defined differently in different implementations we need to avoid the alias and provide
    /// constructors for unsigned long and unsigned long long independently.
    constexpr Value(unsigned int v) noexcept : i64{v} {}
    constexpr Value(unsigned long v) noexcept : i64{v} {}
    constexpr Value(unsigned long long v) noexcept : i64{v} {}
    constexpr Value(int64_t v) noexcept : i64{static_cast<uint64_t>(v)} {}
    constexpr Value(int32_t v) noexcept : i64{static_cast<uint32_t>(v)} {}

    constexpr Value(float v) noexcept : f32{v} {}
    constexpr Value(double v) noexcept : f64{v} {}

    /// Converting constructor from any other type (including smaller integer types) is deleted.
    template <typename T>
    constexpr Value(T) = delete;

    /// Get the value as the given type. Handy in templates.
    /// Only required specializations are provided.
    template <typename T>
    constexpr T as() const noexcept;
};

template <>
constexpr uint64_t Value::as<uint64_t>() const noexcept
{
    return i64;
}

template <>
constexpr uint32_t Value::as<uint32_t>() const noexcept
{
    assert((i64 & 0xffffffff00000000) == 0);
    return static_cast<uint32_t>(i64);
}

template <>
constexpr int64_t Value::as<int64_t>() const noexcept
{
    return static_cast<int64_t>(i64);
}

template <>
constexpr int32_t Value::as<int32_t>() const noexcept
{
    assert((i64 & 0xffffffff00000000) == 0);
    return static_cast<int32_t>(i64);
}

template <>
constexpr float Value::as<float>() const noexcept
{
    return f32;
}

template <>
constexpr double Value::as<double>() const noexcept
{
    return f64;
}
}  // namespace fizzy
