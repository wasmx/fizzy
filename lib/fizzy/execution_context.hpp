// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <limits>

namespace fizzy
{
/// The storage for information shared by calls in the same execution "thread".
/// Users may decide how to allocate the execution context, but some good defaults are available.
class ExecutionContext
{
    /// Call local execution context.
    /// It will automatically decrement the call depth to the original value
    /// when going out of scope.
    class [[nodiscard]] LocalContext
    {
        ExecutionContext& m_shared_ctx;  ///< Reference to the shared execution context.

    public:
        LocalContext(const LocalContext&) = delete;
        LocalContext(LocalContext&&) = delete;
        LocalContext& operator=(const LocalContext&) = delete;
        LocalContext& operator=(LocalContext&&) = delete;

        explicit LocalContext(ExecutionContext& ctx) noexcept : m_shared_ctx{ctx}
        {
            ++m_shared_ctx.depth;
        }

        ~LocalContext() noexcept { --m_shared_ctx.depth; }
    };

public:
    int depth = 0;  ///< Current call depth.
    /// Current ticks left for execution, if #metering_enabled is true.
    /// Execution traps when running out of ticks.
    /// Ignored if #metering_enabled is false.
    int64_t ticks = std::numeric_limits<int64_t>::max();
    /// Set to true to enable execution metering.
    bool metering_enabled = false;

    /// Increments the call depth and returns the local call context which
    /// decrements the call depth back to the original value when going out of scope.
    LocalContext create_local_context() noexcept { return LocalContext{*this}; }
};
}  // namespace fizzy
