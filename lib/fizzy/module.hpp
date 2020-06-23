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
    // Types of globals defined in import section
    std::vector<GlobalType> imported_global_types;

    const FuncType& get_function_type(FuncIdx idx) const noexcept
    {
        assert(idx < imported_function_types.size() + funcsec.size());

        if (idx < imported_function_types.size())
            return imported_function_types[idx];

        const auto type_idx = funcsec[idx - imported_function_types.size()];
        assert(type_idx < typesec.size());

        return typesec[type_idx];
    }

    size_t get_function_count() const noexcept
    {
        return imported_function_types.size() + funcsec.size();
    }

    size_t get_global_count() const noexcept
    {
        return imported_global_types.size() + globalsec.size();
    }

    bool has_table() const noexcept { return !tablesec.empty() || !imported_table_types.empty(); }

    bool has_memory() const noexcept
    {
        return !memorysec.empty() || !imported_memory_types.empty();
    }

    bool is_global_mutable(GlobalIdx idx) const noexcept
    {
        assert(idx < get_global_count());
        return idx < imported_global_types.size() ?
                   imported_global_types[idx].is_mutable :
                   globalsec[idx - imported_global_types.size()].type.is_mutable;
    }
};
}  // namespace fizzy
