// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#if __cplusplus > 201703L

#include <span>

namespace fizzy
{
template <typename T>
using span = std::span<T>;
}

#else

#include <iterator>
#include <type_traits>

namespace fizzy
{
/// The span describes an object that can refer to a contiguous sequence of objects with the first
/// element of the sequence at position zero.
///
/// This is minimal implementation of C++20's std::span:
/// https://en.cppreference.com/w/cpp/container/span
/// Only `const T` is supported.
template <typename T, typename = typename std::enable_if_t<std::is_const_v<T>>>
class span
{
    T* const m_begin = nullptr;
    const std::size_t m_size = 0;

public:
    using value_type = std::remove_cv_t<T>;
    using iterator = T*;
    using reverse_iterator = std::reverse_iterator<iterator>;

    constexpr span() = default;
    constexpr span(const span&) = default;

    constexpr span(T* begin, std::size_t size) noexcept : m_begin{begin}, m_size{size} {}

    /// Constructor from a container type. Requires the Container to implement std::data() and
    /// std::size().
    template <typename Container>
    constexpr span(const Container& container) noexcept
      : m_begin{std::data(container)}, m_size{std::size(container)}
    {}

    constexpr T& operator[](std::size_t index) const noexcept { return m_begin[index]; }

    [[nodiscard]] constexpr std::size_t size() const noexcept { return m_size; }

    constexpr iterator begin() const noexcept { return m_begin; }
    constexpr iterator end() const noexcept { return m_begin + m_size; }

    constexpr reverse_iterator rbegin() const noexcept { return reverse_iterator{end()}; }
    constexpr reverse_iterator rend() const noexcept { return reverse_iterator{begin()}; }
};
}  // namespace fizzy

#endif /* __cplusplus > 201703L */
