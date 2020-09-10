// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "cxx20/span.hpp"
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


/// Contains current frame's locals (including arguments) and operand stack.
/// The storage space for locals and operand stack together is allocated as a
/// continuous memory. Elements occupy the storage space in the order:
/// arguments, local variables, operand stack. Arguments and local variables can
/// be accessed under a single namespace using local() method and are separate
/// from the stack itself.
class OperandStack
{
    /// The size of the pre-allocated internal storage: 128 bytes.
    static constexpr auto small_storage_size = 128 / sizeof(Value);

    /// The pointer to the top item of the operand stack,
    /// or below the stack bottom if stack is empty.
    ///
    /// This pointer always alias m_locals, but it is kept as the first field
    /// because it is accessed the most. Therefore, it must be initialized
    /// in the constructor after the m_locals.
    Value* m_top;

    /// The pointer to the beginning of the locals array.
    /// This always points to one of the storages.
    Value* m_locals;

    /// The pointer to the bottom of the operand stack.
    Value* m_bottom;

    /// The pre-allocated internal storage.
    Value m_small_storage[small_storage_size];

    /// The unbounded storage for items.
    std::unique_ptr<Value[]> m_large_storage;

public:
    /// Default constructor.
    ///
    /// Based on required storage space decides to use small pre-allocated
    /// storage or allocate large storage.
    /// Sets the top stack operand pointer to below the operand stack bottom.
    /// @param args                 Function arguments. Values are copied at the beginning of the
    ///                             storage space.
    /// @param num_local_variables  The number of the function local variables (excluding
    ///                             arguments). This number of values is zeroed in the storage space
    ///                             after the arguments.
    /// @param max_stack_height     The maximum operand stack height in the function. This excludes
    ///                             args and num_local_variables.
    OperandStack(span<const Value> args, size_t num_local_variables, size_t max_stack_height)
    {
        const auto num_args = args.size();
        const auto storage_size_required = num_args + num_local_variables + max_stack_height;

        if (storage_size_required <= small_storage_size)
        {
            m_locals = &m_small_storage[0];
        }
        else
        {
            m_large_storage = std::make_unique<Value[]>(storage_size_required);
            m_locals = &m_large_storage[0];
        }

        m_bottom = m_locals + num_args + num_local_variables;
        m_top = m_bottom - 1;

        std::copy(std::begin(args), std::end(args), m_locals);
        std::fill_n(m_locals + num_args, num_local_variables, 0);
    }

    OperandStack(const OperandStack&) = delete;
    OperandStack& operator=(const OperandStack&) = delete;

    Value& local(size_t index) noexcept
    {
        assert(m_locals + index < m_bottom);
        return m_locals[index];
    }

    /// The current number of items on the stack (aka stack height).
    size_t size() const noexcept { return static_cast<size_t>(m_top + 1 - m_bottom); }

    /// Returns the reference to the top item.
    /// Requires non-empty stack.
    Value& top() noexcept
    {
        assert(size() != 0);
        return *m_top;
    }

    /// Returns the reference to the stack item on given position from the stack top.
    /// Requires index < size().
    Value& operator[](size_t index) noexcept
    {
        assert(index < size());
        return *(m_top - index);
    }

    /// Pushes an item on the stack.
    /// The stack max height limit is not checked.
    void push(Value item) noexcept { *++m_top = item; }

    /// Returns an item popped from the top of the stack.
    /// Requires non-empty stack.
    Value pop() noexcept
    {
        assert(size() != 0);
        return *m_top--;
    }

    void drop(size_t num) noexcept
    {
        assert(num <= size());
        m_top -= num;
    }

    /// Returns iterator to the bottom of the stack.
    const Value* rbegin() const noexcept { return m_bottom; }

    /// Returns end iterator counting from the bottom of the stack.
    const Value* rend() const noexcept { return m_top + 1; }
};
}  // namespace fizzy
