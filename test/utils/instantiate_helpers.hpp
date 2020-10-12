// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "instantiate.hpp"

namespace fizzy::test
{
inline std::unique_ptr<Instance> instantiate(Module module,
    std::vector<ExternalFunction> imported_functions = {},
    std::vector<ExternalTable> imported_tables = {},
    std::vector<ExternalMemory> imported_memories = {},
    std::vector<ExternalGlobal> imported_globals = {},
    uint32_t memory_pages_limit = DefaultMemoryPagesLimit)
{
    return instantiate(std::make_unique<Module>(std::move(module)), std::move(imported_functions),
        std::move(imported_tables), std::move(imported_memories), std::move(imported_globals),
        memory_pages_limit);
}
}  // namespace fizzy::test
