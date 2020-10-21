// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "execute.hpp"
#include <test/utils/instantiate_helpers.hpp>
#include <initializer_list>

namespace fizzy::test
{
inline ExecutionResult execute(const std::unique_ptr<const Module>& module, FuncIdx func_idx,
    std::initializer_list<Value> args) noexcept
{
    auto instance = instantiate(*module);
    return execute(*instance, func_idx, args);
}
}  // namespace fizzy::test
