// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <vector>

namespace fizzy
{
template <typename T>
class Stack : public std::vector<T>
{
public:
    using difference_type = typename std::vector<T>::difference_type;

    using std::vector<T>::vector;

    using std::vector<T>::back;
    using std::vector<T>::emplace_back;
    using std::vector<T>::pop_back;
    using std::vector<T>::resize;
    using std::vector<T>::size;

    // Also used: size(), resize(), clear(), empty(), end()

    void push(T val) { emplace_back(val); }

    T pop()
    {
        const auto res = back();
        pop_back();
        return res;
    }

    T peek(size_t depth = 0) const noexcept { return (*this)[size() - depth - 1]; }

    /// Drops @a num_elements elements from the top of the stack.
    void drop(size_t num_elements = 1) noexcept { resize(size() - num_elements); }
};
}  // namespace fizzy
