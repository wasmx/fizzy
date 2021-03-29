// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "cxx20/span.hpp"
#include "exceptions.hpp"
#include "instantiate.hpp"
#include "limits.hpp"
#include "module.hpp"
#include "types.hpp"
#include "value.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace fizzy
{
/// The result of an execution.
struct ExecutionResult
{
    /// This is true if the execution has trapped.
    const bool trapped = false;
    /// This is true if value contains valid data.
    const bool has_value = false;
    /// The result value. Valid if `has_value == true`.
    const Value value{};

    /// Constructs result with a value.
    constexpr ExecutionResult(Value _value) noexcept : has_value{true}, value{_value} {}

    /// Constructs result in "void" or "trap" state depending on the success flag.
    /// Prefer using Void and Trap constants instead.
    constexpr explicit ExecutionResult(bool success) noexcept : trapped{!success} {}
};

/// Shortcut for execution that resulted in successful execution, but without a result.
constexpr ExecutionResult Void{true};
/// Shortcut for execution that resulted in a trap.
constexpr ExecutionResult Trap{false};

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
        explicit Guard(ExecutionContext& ctx) noexcept : m_execution_context{ctx} {}
        ~Guard() noexcept { --m_execution_context.depth; }
    };

public:
    int depth = 0;  ///< Current call depth.

    /// Increments the call depth and returns the guard object which decrements
    /// the call depth back to the original value when going out of scope.
    Guard increment_call_depth() noexcept
    {
        ++depth;
        return Guard{*this};
    }
};

/// Execute a function from an instance.
///
/// @param  instance    The instance.
/// @param  func_idx    The function index. MUST be a valid index, otherwise undefined behaviour
///                     (including crash) happens.
/// @param  args        The pointer to the arguments. The number of items and their types must match
///                     the expected number of input parameters of the function, otherwise undefined
///                     behaviour (including crash) happens.
/// @param  ctx         Execution context.
/// @return             The result of the execution.
ExecutionResult execute(
    Instance& instance, FuncIdx func_idx, const Value* args, ExecutionContext& ctx) noexcept;

/// Execute a function from an instance with execution context starting with default depth of 0.
/// Arguments and behavior is the same as in the other execute().
inline ExecutionResult execute(Instance& instance, FuncIdx func_idx, const Value* args) noexcept
{
    ExecutionContext ctx;
    return execute(instance, func_idx, args, ctx);
}
}  // namespace fizzy
