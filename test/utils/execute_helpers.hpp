// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "execute.hpp"

namespace fizzy::test
{
inline execution_result execute(const Module& module, FuncIdx func_idx, std::vector<uint64_t> args)
{
    auto instance = instantiate(module);
    return execute(*instance, func_idx, std::move(args));
}
}  // namespace fizzy::test