#pragma once

#include "exceptions.hpp"
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
    std::function<execution_result(Instance&, std::vector<uint64_t>)> function;
    FuncType type;
};

struct TableFunction
{
    std::function<execution_result(std::vector<uint64_t>, int depth)> function;
    FuncType type;
};

using table_elements = std::vector<std::optional<TableFunction>>;
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
    size_t memory_max_pages = 0;
    // Table is either allocated and owned by the instance or imported and owned externally.
    // For these cases unique_ptr would either have a normal deleter or noop deleter respectively.
    table_ptr table = {nullptr, [](table_elements*) {}};
    std::vector<uint64_t> globals;
    std::vector<ExternalFunction> imported_functions;
    std::vector<ExternalGlobal> imported_globals;

    Instance(Module _module, bytes_ptr _memory, size_t _memory_max_pages, table_ptr _table,
        std::vector<uint64_t> _globals, std::vector<ExternalFunction> _imported_functions,
        std::vector<ExternalGlobal> _imported_globals)
      : module(std::move(_module)),
        memory(std::move(_memory)),
        memory_max_pages(_memory_max_pages),
        table(std::move(_table)),
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
    Instance& instance, FuncIdx func_idx, std::vector<uint64_t> args, int depth = 0);

// TODO: remove this helper
execution_result execute(const Module& module, FuncIdx func_idx, std::vector<uint64_t> args);

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
