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

/// The "unsafe" internal execute function.
Value* execute_internal(Instance& instance, FuncIdx func_idx, Value* args, int depth);

// Execute a function on an instance.
inline ExecutionResult execute(
    Instance& instance, FuncIdx func_idx, const Value* args, int depth = 0)
{
    const auto& func_type = instance.module->get_function_type(func_idx);
    const auto num_args = func_type.inputs.size();
    const auto num_outputs = func_type.outputs.size();
    assert(num_outputs <= 1);

    const auto arg0 = num_args >= 1 ? args[0] : Value{};

    Value fake_arg;
    auto* p_args = num_args == 0 ? &fake_arg : const_cast<Value*>(args);

    auto* args_end = p_args + num_args;
    const auto res = execute_internal(instance, func_idx, args_end, depth);

    if (res == nullptr)
        return Trap;

    if (num_outputs == 1)
    {
        // Restore original value, because the caller does not expect it being modified.
        const auto result_value = p_args[0];
        p_args[0] = arg0;
        return ExecutionResult(result_value);
    }

    return Void;
}

inline ExecutionResult execute(
    Instance& instance, FuncIdx func_idx, std::initializer_list<Value> args)
{
    assert(args.size() == instance.module->get_function_type(func_idx).inputs.size());
    return execute(instance, func_idx, args.begin());
}
}  // namespace fizzy
