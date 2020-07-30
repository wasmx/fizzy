// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace fizzy
{
union Value
{
    uint64_t i64;

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

    /// Converting constructor from any other type (including smaller integer types) is deleted.
    template <typename T>
    constexpr Value(T) = delete;

    constexpr operator uint64_t() const noexcept { return i64; }
};
}  // namespace fizzy
