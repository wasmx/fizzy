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
#include <any>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

namespace fizzy
{
struct ExecutionResult;
struct Instance;

/// Function pointer to the execution function.
using HostFunctionPtr = ExecutionResult (*)(
    std::any& host_context, Instance&, const Value* args, int depth);

/// Function representing either WebAssembly or host function execution.
class ExecuteFunction
{
    /// Pointer to WebAssembly function instance.
    /// Equals nullptr in case this ExecuteFunction represents host function.
    Instance* m_instance = nullptr;

    /// Index of WebAssembly function.
    /// Equals 0 in case this ExecuteFunction represents host function.
    FuncIdx m_func_idx = 0;

    /// Pointer to a host function.
    /// Equals nullptr in case this ExecuteFunction represents WebAssembly function.
    HostFunctionPtr m_host_function = nullptr;

    /// Opaque context of host function execution, which is passed to it as host_context parameter.
    /// Doesn't have value in case this ExecuteFunction represents WebAssembly function.
    std::any m_host_context;

public:
    // WebAssembly function constructor.
    ExecuteFunction(Instance& instance, FuncIdx func_idx) noexcept
      : m_instance{&instance}, m_func_idx{func_idx}
    {}

    /// Host function constructor without context.
    /// The function will always be called with empty host_context argument.
    ExecuteFunction(HostFunctionPtr f) noexcept : m_host_function{f} {}

    /// Host function constructor with context.
    /// The function will be called with a reference to @a host_context.
    /// Copies of the function will have their own copy of @a host_context.
    ExecuteFunction(HostFunctionPtr f, std::any host_context)
      : m_host_function{f}, m_host_context{std::move(host_context)}
    {}

    /// Function call operator.
    ExecutionResult operator()(Instance& instance, const Value* args, int depth);

    /// Function pointer stored inside this object.
    HostFunctionPtr get_host_function() const noexcept { return m_host_function; }
};

/// Function with associated input/output types,
/// used to represent both imported and exported functions.
struct ExternalFunction
{
    ExecuteFunction function;
    span<const ValType> input_types;
    span<const ValType> output_types;

    ExternalFunction(ExecuteFunction _function, span<const ValType> _input_types,
        span<const ValType> _output_types)
      : function(std::move(_function)), input_types(_input_types), output_types(_output_types)
    {}

    ExternalFunction(ExecuteFunction _function, const FuncType& type)
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
    /// Module of this instance.
    std::unique_ptr<const Module> module;

    /// Instance memory.
    /// Memory is either allocated and owned by the instance or imported as already allocated bytes
    /// and owned externally.
    /// For these cases unique_ptr would either have a normal deleter or no-op deleter respectively
    bytes_ptr memory = {nullptr, [](bytes*) {}};

    /// Memory limits.
    Limits memory_limits;

    /// Hard limit for memory growth in pages, checked when memory is defined as unbounded in
    /// module.
    uint32_t memory_pages_limit = 0;

    /// Instance table.
    /// Table is either allocated and owned by the instance or imported and owned externally.
    /// For these cases unique_ptr would either have a normal deleter or no-op deleter respectively.
    table_ptr table = {nullptr, [](table_elements*) {}};

    /// Table limits.
    Limits table_limits;

    /// Instance globals (excluding imported globals).
    std::vector<Value> globals;

    /// Imported functions.
    std::vector<ExternalFunction> imported_functions;

    /// Imported globals.
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
    /// Module name.
    std::string module;

    /// Function name.
    std::string name;

    /// Array of input types.
    std::vector<ValType> inputs;

    /// Output type or empty optional if function has void return type.
    std::optional<ValType> output;

    /// Function object, which will be called to execute the function.
    ExecuteFunction function;
};

/// Create vector of fizzy::ExternalFunction ready to be passed to instantiate().
/// @a imported_functions may be in any order, but must contain functions for all of the imported
/// function names defined in the module.
std::vector<ExternalFunction> resolve_imported_functions(
    const Module& module, const std::vector<ImportedFunction>& imported_functions);

/// Global that should be used by instantiate as import, identified by module and global name.
struct ImportedGlobal
{
    /// Module name.
    std::string module;

    /// Global name.
    std::string name;

    /// Pointer to global value.
    Value* value = nullptr;

    /// Value type of global.
    ValType type = ValType::i32;

    /// Mutability of global.
    bool is_mutable = false;
};

/// Create vector of fizzy::ExternalGlobal ready to be passed to instantiate().
/// @a imported_globals may be in any order, but must contain globals for all of the imported global
/// names defined in the module.
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
