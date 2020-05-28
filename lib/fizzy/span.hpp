// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <type_traits>
#include <vector>

namespace fizzy
{
/// The span describes an object that can refer to a contiguous sequence of objects with the first
/// element of the sequence at position zero.
///
/// This is minimal implementation of C++20's std::span:
/// https://en.cppreference.com/w/cpp/container/span
/// Only `const T` is supported.
template <typename T, typename = typename std::enable_if<std::is_const_v<T>>::type>
struct span
{
private:
    const T* const m_begin = nullptr;
    const std::size_t m_size = 0;

public:
    constexpr span() noexcept = default;
    constexpr span(const span&) = default;

    constexpr span(const T* begin, std::size_t size) noexcept : m_begin{begin}, m_size{size} {}

    constexpr span(std::initializer_list<typename std::remove_const_t<T>> init) noexcept
      : m_begin{std::data(init)}, m_size{std::size(init)}
    {}

    constexpr span(const std::vector<typename std::remove_const_t<T>>& container) noexcept
      : m_begin{std::data(container)}, m_size{std::size(container)}
    {}

    const T& operator[](std::size_t index) const noexcept { return m_begin[index]; }

    const T* begin() const noexcept { return m_begin; }
    const T* end() const noexcept { return m_begin + m_size; }

    [[nodiscard]] std::size_t size() const noexcept { return m_size; }
};
}  // namespace fizzy
