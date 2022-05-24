// Fizzy: A fast WebAssembly interpreter
// Copyright 2022 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <type_traits>

namespace fizzy
{
/// C++14 version of the proposed C++23 feature
/// https://en.cppreference.com/w/cpp/utility/to_underlying
template <typename T>
constexpr std::underlying_type_t<T> to_underlying(T value) noexcept
{
    return static_cast<std::underlying_type_t<T>>(value);
}
}  // namespace fizzy
