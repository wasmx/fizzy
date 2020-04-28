// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "types.hpp"
#include <cassert>
#include <optional>
#include <vector>

namespace fizzy
{
struct Module
{
    // https://webassembly.github.io/spec/core/binary/modules.html#type-section
    std::vector<FuncType> typesec;
    // https://webassembly.github.io/spec/core/binary/modules.html#import-section
    std::vector<Import> importsec;
    // https://webassembly.github.io/spec/core/binary/modules.html#function-section
    std::vector<TypeIdx> funcsec;
    // https://webassembly.github.io/spec/core/binary/modules.html#table-section
    std::vector<Table> tablesec;
    // https://webassembly.github.io/spec/core/binary/modules.html#memory-section
    std::vector<Memory> memorysec;
    // https://webassembly.github.io/spec/core/binary/modules.html#global-section
    std::vector<Global> globalsec;
    // https://webassembly.github.io/spec/core/binary/modules.html#export-section
    std::vector<Export> exportsec;
    // https://webassembly.github.io/spec/core/binary/modules.html#start-section
    std::optional<FuncIdx> startfunc;
    // https://webassembly.github.io/spec/core/binary/modules.html#element-section
    std::vector<Element> elementsec;
    // https://webassembly.github.io/spec/core/binary/modules.html#code-section
    std::vector<Code> codesec;
    // https://webassembly.github.io/spec/core/binary/modules.html#data-section
    std::vector<Data> datasec;

    // Types of functions defined in import section
    std::vector<FuncType> imported_function_types;
    // Types of tables defined in import section
    std::vector<Table> imported_table_types;
    // Types of memories defined in import section
    std::vector<Memory> imported_memory_types;
    // Mutability of globals defined in import section
    std::vector<bool> imported_globals_mutability;
};

inline const FuncType& function_type(const Module& module, FuncIdx idx) noexcept
{
    assert(idx < module.imported_function_types.size() + module.funcsec.size());

    if (idx < module.imported_function_types.size())
        return module.imported_function_types[idx];

    const auto type_idx = module.funcsec[idx - module.imported_function_types.size()];
    assert(type_idx < module.typesec.size());

    return module.typesec[type_idx];
}
}  // namespace fizzy
