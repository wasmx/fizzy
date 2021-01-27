// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "cxx20/span.hpp"
#include "exceptions.hpp"
#include "limits.hpp"
#include "module.hpp"
#include "types.hpp"
#include "value.hpp"
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace fizzy
{
struct ExecutionResult;
struct Instance;

/// Function representing WebAssembly or host function execution.
using execute_function = std::function<ExecutionResult(Instance&, const Value*, int depth)>;

/// Function with associated input/output types,
/// used to represent both imported and exported functions.
struct ExternalFunction
{
    execute_function function;
    span<const ValType> input_types;
    span<const ValType> output_types;

    ExternalFunction(execute_function _function, span<const ValType> _input_types,
        span<const ValType> _output_types)
      : function(std::move(_function)), input_types(_input_types), output_types(_output_types)
    {}

    ExternalFunction(execute_function _function, const FuncType& type)
      : function(std::move(_function)), input_types(type.inputs), output_types(type.outputs)
    {}
};

/// Table element, which references a function in any instance.
struct TableElement
{
    /// Pointer to function's instance or nullptr when table element is not initialized.
    Instance* instance = nullptr;
    /// Index of the function in instance.
    FuncIdx func_idx = 0;
    /// This pointer is empty most of the time and is used only to keep instance alive in one edge
    /// case, when start function traps, but instantiate has already modified some elements of a
    /// shared (imported) table.
    std::shared_ptr<Instance> shared_instance;
};

using table_elements = std::vector<TableElement>;
using table_ptr = std::unique_ptr<table_elements, void (*)(table_elements*)>;

struct ExternalTable
{
    table_elements* table = nullptr;
    Limits limits;
};

struct ExternalMemory
{
    bytes* data = nullptr;
    Limits limits;
};

struct ExternalGlobal
{
    Value* value = nullptr;
    GlobalType type;
};

using bytes_ptr = std::unique_ptr<bytes, void (*)(bytes*)>;

/// The module instance.
struct Instance
{
    std::unique_ptr<const Module> module;
    // Memory is either allocated and owned by the instance or imported as already allocated bytes
    // and owned externally.
    // For these cases unique_ptr would either have a normal deleter or noop deleter respectively
    bytes_ptr memory = {nullptr, [](bytes*) {}};
    Limits memory_limits;
    // Hard limit for memory growth in pages, checked when memory is defined as unbounded in module
    uint32_t memory_pages_limit = 0;
    // Table is either allocated and owned by the instance or imported and owned externally.
    // For these cases unique_ptr would either have a normal deleter or noop deleter respectively.
    table_ptr table = {nullptr, [](table_elements*) {}};
    Limits table_limits;
    std::vector<Value> globals;
    std::vector<ExternalFunction> imported_functions;
    std::vector<ExternalGlobal> imported_globals;

    Instance(std::unique_ptr<const Module> _module, bytes_ptr _memory, Limits _memory_limits,
        uint32_t _memory_pages_limit, table_ptr _table, Limits _table_limits,
        std::vector<Value> _globals, std::vector<ExternalFunction> _imported_functions,
        std::vector<ExternalGlobal> _imported_globals)
      : module(std::move(_module)),
        memory(std::move(_memory)),
        memory_limits(_memory_limits),
        memory_pages_limit(_memory_pages_limit),
        table(std::move(_table)),
        table_limits(_table_limits),
        globals(std::move(_globals)),
        imported_functions(std::move(_imported_functions)),
        imported_globals(std::move(_imported_globals))
    {}
};

/// Instantiate a module.
std::unique_ptr<Instance> instantiate(std::unique_ptr<const Module> module,
    std::vector<ExternalFunction> imported_functions = {},
    std::vector<ExternalTable> imported_tables = {},
    std::vector<ExternalMemory> imported_memories = {},
    std::vector<ExternalGlobal> imported_globals = {},
    uint32_t memory_pages_limit = DefaultMemoryPagesLimit);

/// Function that should be used by instantiate as import, identified by module and function name.
struct ImportedFunction
{
    std::string module;
    std::string name;
    std::vector<ValType> inputs;
    std::optional<ValType> output;
    execute_function function;
};

/// Create vector of ExternalFunctions ready to be passed to instantiate.
/// @a imported_functions may be in any order, but must contain functions for all of the imported
/// function names defined in the module.
std::vector<ExternalFunction> resolve_imported_functions(
    const Module& module, const std::vector<ImportedFunction>& imported_functions);

// Global that should be used by instantiate as import, identified by module and global name.
struct ImportedGlobal
{
    std::string module;
    std::string name;
    Value* value = nullptr;
    ValType type = ValType::i32;
    bool is_mutable = false;
};

// Create vector of ExternalGlobals ready to be passed to instantiate.
// imported_globals may be in any order, but must contain globals for all of the imported global
// names defined in the module.
std::vector<ExternalGlobal> resolve_imported_globals(
    const Module& module, const std::vector<ImportedGlobal>& imported_globals);

/// Find exported function index by name.
std::optional<FuncIdx> find_exported_function(const Module& module, std::string_view name);

/// Find exported function by name.
std::optional<ExternalFunction> find_exported_function(Instance& instance, std::string_view name);

/// Find exported global by name.
std::optional<ExternalGlobal> find_exported_global(Instance& instance, std::string_view name);

/// Find exported table by name.
std::optional<ExternalTable> find_exported_table(Instance& instance, std::string_view name);

/// Find exported memory by name.
std::optional<ExternalMemory> find_exported_memory(Instance& instance, std::string_view name);

}  // namespace fizzy
