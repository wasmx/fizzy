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
    constexpr Value(uint64_t v) noexcept : i64{v} {}

    constexpr operator uint64_t() const noexcept { return i64; }
};
}  // namespace fizzy
