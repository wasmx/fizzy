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

    const FuncType& get_function_type(FuncIdx idx) const noexcept
    {
        assert(idx < imported_function_types.size() + funcsec.size());

        if (idx < imported_function_types.size())
            return imported_function_types[idx];

        const auto type_idx = funcsec[idx - imported_function_types.size()];
        assert(type_idx < typesec.size());

        return typesec[type_idx];
    }

    bool has_memory() const noexcept
    {
        return !memorysec.empty() || !imported_memory_types.empty();
    }
};
}  // namespace fizzy
