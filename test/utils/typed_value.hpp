// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include "types.hpp"
#include "value.hpp"

namespace fizzy::test
{
/// Creates uint64_t value with intention to map it to i64 WebAssembly type.
inline constexpr uint64_t operator"" _u64(unsigned long long x)
{
    return uint64_t{x};  // Guarantees lossless conversion if needed.
}

/// Creates uint32_t value with intention to map it to i32 WebAssembly type.
inline constexpr uint32_t operator"" _u32(unsigned long long x)
{
    const auto v = static_cast<uint32_t>(x);
    if (v != x)
        throw "integer literal is too large to be represented in the u32 type";
    return v;
}

struct TypedValue
{
    Value value;
    ValType type;

    /// Assign type to the typeless Value.
    /// No validation is possible, so the correctness is on the user only.
    constexpr TypedValue(ValType ty, Value v) noexcept : value{v}, type{ty} {}

    constexpr TypedValue(int32_t v) noexcept : TypedValue{ValType::i32, v} {}
    constexpr TypedValue(uint32_t v) noexcept : TypedValue{ValType::i32, v} {}
    constexpr TypedValue(int64_t v) noexcept : TypedValue{ValType::i64, v} {}
    constexpr TypedValue(uint64_t v) noexcept : TypedValue{ValType::i64, v} {}
    constexpr TypedValue(float v) noexcept : TypedValue{ValType::f32, v} {}
    constexpr TypedValue(double v) noexcept : TypedValue{ValType::f64, v} {}
};
}  // namespace fizzy::test
