// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "execute.hpp"
#include <initializer_list>

namespace fizzy::test
{
inline ExecutionResult execute(
    const Module& module, FuncIdx func_idx, std::initializer_list<Value> args)
{
    auto instance = instantiate(module);
    return execute(*instance, func_idx, args);
}
}  // namespace fizzy::test