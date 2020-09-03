// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "value.hpp"
#include <cassert>
#include <cstdint>
#include <memory>
#include <vector>

namespace fizzy
{
template <typename T>
class Stack
{
    std::vector<T> m_container;

public:
    void push(T val) { m_container.emplace_back(val); }

    template <typename... Args>
    void emplace(Args&&... args)
    {
        m_container.emplace_back(std::forward<Args>(args)...);
    }

    T pop() noexcept
    {
        assert(!m_container.empty());
        const auto res = m_container.back();
        m_container.pop_back();
        return res;
    }

    bool empty() const noexcept { return m_container.empty(); }

    size_t size() const noexcept { return m_container.size(); }

    T& operator[](size_t index) noexcept { return m_container[size() - index - 1]; }

    T& top() noexcept { return m_container.back(); }

    void shrink(size_t new_size) noexcept
    {
        assert(new_size <= size());
        m_container.resize(new_size);
    }
};

class OperandStack
{
    /// The size of the pre-allocated internal storage: 128 bytes.
    static constexpr auto small_storage_size = 128 / sizeof(Value);

    /// The pointer to the top item, or below the stack bottom if stack is empty.
    ///
    /// This pointer always alias m_storage, but it is kept as the first field
    /// because it is accessed the most. Therefore, it must be initialized
    /// in the constructor after the m_storage.
    Value* m_top;

    /// The pre-allocated internal storage.
    Value m_small_storage[small_storage_size];

    /// The unbounded storage for items.
    std::unique_ptr<Value[]> m_large_storage;

    Value* bottom() noexcept { return m_large_storage ? m_large_storage.get() : m_small_storage; }

    const Value* bottom() const noexcept
    {
        return m_large_storage ? m_large_storage.get() : m_small_storage;
    }

public:
    /// Default constructor.
    ///
    /// Based on @p max_stack_height decides if to use small pre-allocated storage or allocate
    /// large storage.
    /// Sets the top item pointer to below the stack bottom.
    explicit OperandStack(size_t max_stack_height)
    {
        if (max_stack_height > small_storage_size)
            m_large_storage = std::make_unique<Value[]>(max_stack_height);
        m_top = bottom() - 1;
    }

    OperandStack(const OperandStack&) = delete;
    OperandStack& operator=(const OperandStack&) = delete;

    /// The current number of items on the stack (aka stack height).
    size_t size() const noexcept { return static_cast<size_t>(m_top + 1 - bottom()); }

    /// Returns the reference to the top item.
    /// Requires non-empty stack.
    auto& top() noexcept
    {
        assert(size() != 0);
        return *m_top;
    }

    /// Returns the reference to the stack item on given position from the stack top.
    /// Requires index < size().
    auto& operator[](size_t index) noexcept
    {
        assert(index < size());
        return *(m_top - index);
    }

    /// Pushes an item on the stack.
    /// The stack max height limit is not checked.
    void push(Value item) noexcept { *++m_top = item; }

    /// Returns an item popped from the top of the stack.
    /// Requires non-empty stack.
    auto pop() noexcept
    {
        assert(size() != 0);
        return *m_top--;
    }

    void drop(size_t num) noexcept
    {
        assert(num <= size());
        m_top -= num;
    }

    /// Shrinks the stack to the given new size by dropping items from the top.
    ///
    /// Requires new_size <= size().
    /// shrink(0) clears entire stack and moves the top pointer below the stack base.
    void shrink(size_t new_size) noexcept
    {
        assert(new_size <= size());
        // For new_size == 0, the m_top will point below the storage.
        m_top = bottom() + new_size - 1;
    }

    /// Returns iterator to the bottom of the stack.
    const Value* rbegin() const noexcept { return bottom(); }

    /// Returns end iterator counting from the bottom of the stack.
    const Value* rend() const noexcept { return m_top + 1; }
};
}  // namespace fizzy
