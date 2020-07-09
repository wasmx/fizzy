// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <utility>

namespace fizzy
{
/// Constexpr vector is used to represent compile time constant value lists.
/// It allows to have a collection (constexpr array) of such lists, where list lengths vary across
/// the collection, but each length is still known at compile time (and doesn't exceed Capacity
/// limit).
template <typename T, std::size_t Capacity>
class constexpr_vector
{
private:
    const T m_array[Capacity];
    const std::size_t m_size;

public:
    template <typename... U>
    constexpr constexpr_vector(U&&... u) noexcept
      : m_array{std::forward<U>(u)...}, m_size(sizeof...(U))
    {}

    constexpr const T* data() const noexcept { return m_array; }
    constexpr std::size_t size() const noexcept { return m_size; }

    constexpr const T* begin() const noexcept { return m_array; }
    constexpr const T* end() const noexcept { return m_array + m_size; }
};

}  // namespace fizzy
