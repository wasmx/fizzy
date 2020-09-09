// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "exceptions.hpp"
#include "instantiate.hpp"
#include "limits.hpp"
#include "module.hpp"
#include "span.hpp"
#include "types.hpp"
#include "value.hpp"
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace fizzy
{
// The result of an execution.
struct ExecutionResult
{
    const bool trapped = false;
    const bool has_value = false;
    const Value value{};

    /// Constructs result with a value.
    constexpr ExecutionResult(Value _value) noexcept : has_value{true}, value{_value} {}

    /// Constructs result in "void" or "trap" state depending on the success flag.
    /// Prefer using Void and Trap constants instead.
    constexpr explicit ExecutionResult(bool success) noexcept : trapped{!success} {}
};

constexpr ExecutionResult Void{true};
constexpr ExecutionResult Trap{false};

// Execute a function on an instance.
ExecutionResult execute(
    Instance& instance, FuncIdx func_idx, span<const Value> args, int depth = 0) noexcept;

inline ExecutionResult execute(
    Instance& instance, FuncIdx func_idx, std::initializer_list<Value> args) noexcept
{
    return execute(instance, func_idx, span<const Value>{args});
}
}  // namespace fizzy
