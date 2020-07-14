// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "exceptions.hpp"
#include "module.hpp"
#include "span.hpp"
#include "types.hpp"
#include <cstdint>
#include <functional>
#include <memory>

namespace fizzy
{
// The result of an execution.
struct execution_result
{
    // true if execution resulted in a trap
    bool trapped;
    // the resulting stack (e.g. return values)
    // NOTE: this can be either 0 or 1 items
    std::vector<uint64_t> stack;
};

struct Instance;

struct ExternalFunction
{
    std::function<execution_result(Instance&, span<const uint64_t>, int depth)> function;
    FuncType type;
};

using table_elements = std::vector<std::optional<ExternalFunction>>;
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
    uint64_t* value = nullptr;
    bool is_mutable = false;
};

using bytes_ptr = std::unique_ptr<bytes, void (*)(bytes*)>;

// The module instance.
struct Instance
{
    Module module;
    // Memory is either allocated and owned by the instance or imported as already allocated bytes
    // and owned externally.
    // For these cases unique_ptr would either have a normal deleter or noop deleter respectively
    bytes_ptr memory = {nullptr, [](bytes*) {}};
    Limits memory_limits;
    // Table is either allocated and owned by the instance or imported and owned externally.
    // For these cases unique_ptr would either have a normal deleter or noop deleter respectively.
    table_ptr table = {nullptr, [](table_elements*) {}};
    Limits table_limits;
    std::vector<uint64_t> globals;
    std::vector<ExternalFunction> imported_functions;
    std::vector<ExternalGlobal> imported_globals;

    Instance(Module _module, bytes_ptr _memory, Limits _memory_limits, table_ptr _table,
        Limits _table_limits, std::vector<uint64_t> _globals,
        std::vector<ExternalFunction> _imported_functions,
        std::vector<ExternalGlobal> _imported_globals)
      : module(std::move(_module)),
        memory(std::move(_memory)),
        memory_limits(_memory_limits),
        table(std::move(_table)),
        table_limits(_table_limits),
        globals(std::move(_globals)),
        imported_functions(std::move(_imported_functions)),
        imported_globals(std::move(_imported_globals))
    {}
};

// Instantiate a module.
std::unique_ptr<Instance> instantiate(Module module,
    std::vector<ExternalFunction> imported_functions = {},
    std::vector<ExternalTable> imported_tables = {},
    std::vector<ExternalMemory> imported_memories = {},
    std::vector<ExternalGlobal> imported_globals = {});

// Execute a function on an instance.
execution_result execute(
    Instance& instance, FuncIdx func_idx, span<const uint64_t> args, int depth = 0);

inline execution_result execute(
    Instance& instance, FuncIdx func_idx, std::initializer_list<uint64_t> args)
{
    return execute(instance, func_idx, span<const uint64_t>{args});
}


// Function that should be used by instantiate as imports, identified by module and function name.
struct ImportedFunction
{
    std::string module;
    std::string name;
    std::vector<ValType> inputs;
    std::optional<ValType> output;
    std::function<execution_result(Instance&, span<const uint64_t>, int depth)> function;
};

// Create vector of ExternalFunctions ready to be passed to instantiate.
// imported_functions may be in any order,
// but must contain functions for all of the imported function names defined in the module.
std::vector<ExternalFunction> resolve_imported_functions(
    const Module& module, std::vector<ImportedFunction> imported_functions);

// Find exported function index by name.
std::optional<FuncIdx> find_exported_function(const Module& module, std::string_view name);

// Find exported function by name.
std::optional<ExternalFunction> find_exported_function(Instance& instance, std::string_view name);

// Find exported global by name.
std::optional<ExternalGlobal> find_exported_global(Instance& instance, std::string_view name);

// Find exported table by name.
std::optional<ExternalTable> find_exported_table(Instance& instance, std::string_view name);

// Find exported memory by name.
std::optional<ExternalMemory> find_exported_memory(Instance& instance, std::string_view name);

}  // namespace fizzy
