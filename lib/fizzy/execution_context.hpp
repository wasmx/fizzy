// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

namespace fizzy
{
/// The storage for information shared by calls in the same execution "thread".
/// Users may decide how to allocate the execution context, but some good defaults are available.
class ExecutionContext
{
    /// Call depth increment guard.
    /// It will automatically decrement the call depth to the original value
    /// when going out of scope.
    class [[nodiscard]] Guard
    {
        ExecutionContext& m_execution_context;  ///< Reference to the guarded execution context.

    public:
        Guard(const Guard&) = delete;
        Guard(Guard&&) = delete;
        Guard& operator=(const Guard&) = delete;
        Guard& operator=(Guard&&) = delete;

        explicit Guard(ExecutionContext& ctx) noexcept : m_execution_context{ctx}
        {
            ++m_execution_context.depth;
        }

        ~Guard() noexcept { --m_execution_context.depth; }
    };

public:
    int depth = 0;  ///< Current call depth.

    /// Increments the call depth and returns the guard object which decrements
    /// the call depth back to the original value when going out of scope.
    Guard increment_call_depth() noexcept { return Guard{*this}; }
};
}  // namespace fizzy
