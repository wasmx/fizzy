// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "instantiate.hpp"
#include "execute.hpp"  // needed for table elements initialization
#include <algorithm>
#include <cassert>
#include <cstring>

namespace fizzy
{
namespace
{
void match_imported_functions(const std::vector<FuncType>& module_imported_types,
    const std::vector<ExternalFunction>& imported_functions)
{
    if (module_imported_types.size() != imported_functions.size())
    {
        throw instantiate_error{"module requires " + std::to_string(module_imported_types.size()) +
                                " imported functions, " +
                                std::to_string(imported_functions.size()) + " provided"};
    }

    for (size_t i = 0; i < imported_functions.size(); ++i)
    {
        if (module_imported_types[i] != imported_functions[i].type)
        {
            throw instantiate_error{"function " + std::to_string(i) +
                                    " type doesn't match module's imported function type"};
        }
    }
}

void match_limits(const Limits& external_limits, const Limits& module_limits)
{
    if (external_limits.min < module_limits.min)
        throw instantiate_error{"provided import's min is below import's min defined in module"};

    if (!module_limits.max.has_value())
        return;

    if (external_limits.max.has_value() && *external_limits.max <= *module_limits.max)
        return;

    throw instantiate_error{"provided import's max is above import's max defined in module"};
}

void match_imported_tables(const std::vector<Table>& module_imported_tables,
    const std::vector<ExternalTable>& imported_tables)
{
    assert(module_imported_tables.size() <= 1);

    if (imported_tables.size() > 1)
        throw instantiate_error{"only 1 imported table is allowed"};

    if (module_imported_tables.empty())
    {
        if (!imported_tables.empty())
        {
            throw instantiate_error{
                "trying to provide imported table to a module that doesn't define one"};
        }
    }
    else
    {
        if (imported_tables.empty())
            throw instantiate_error{"module defines an imported table but none was provided"};

        match_limits(imported_tables[0].limits, module_imported_tables[0].limits);

        if (imported_tables[0].table == nullptr)
            throw instantiate_error{"provided imported table has a null pointer to data"};

        const auto size = imported_tables[0].table->size();
        const auto min = imported_tables[0].limits.min;
        const auto& max = imported_tables[0].limits.max;
        if (size < min || (max.has_value() && size > *max))
            throw instantiate_error{"provided imported table doesn't fit provided limits"};
    }
}

void match_imported_memories(const std::vector<Memory>& module_imported_memories,
    const std::vector<ExternalMemory>& imported_memories)
{
    assert(module_imported_memories.size() <= 1);

    if (imported_memories.size() > 1)
        throw instantiate_error{"only 1 imported memory is allowed"};

    if (module_imported_memories.empty())
    {
        if (!imported_memories.empty())
        {
            throw instantiate_error{
                "trying to provide imported memory to a module that doesn't define one"};
        }
    }
    else
    {
        if (imported_memories.empty())
            throw instantiate_error{"module defines an imported memory but none was provided"};

        match_limits(imported_memories[0].limits, module_imported_memories[0].limits);

        if (imported_memories[0].data == nullptr)
            throw instantiate_error{"provided imported memory has a null pointer to data"};

        const auto size = imported_memories[0].data->size();
        const auto min = imported_memories[0].limits.min;
        const auto& max = imported_memories[0].limits.max;
        if (size < min * PageSize || (max.has_value() && size > *max * PageSize))
            throw instantiate_error{"provided imported memory doesn't fit provided limits"};
    }
}

void match_imported_globals(const std::vector<GlobalType>& module_imported_globals,
    const std::vector<ExternalGlobal>& imported_globals)
{
    if (module_imported_globals.size() != imported_globals.size())
    {
        throw instantiate_error{
            "module requires " + std::to_string(module_imported_globals.size()) +
            " imported globals, " + std::to_string(imported_globals.size()) + " provided"};
    }

    for (size_t i = 0; i < imported_globals.size(); ++i)
    {
        if (imported_globals[i].type.value_type != module_imported_globals[i].value_type)
        {
            throw instantiate_error{
                "global " + std::to_string(i) + " value type doesn't match module's global type"};
        }
        if (imported_globals[i].type.is_mutable != module_imported_globals[i].is_mutable)
        {
            throw instantiate_error{"global " + std::to_string(i) +
                                    " mutability doesn't match module's global mutability"};
        }
        if (imported_globals[i].value == nullptr)
        {
            throw instantiate_error{"global " + std::to_string(i) + " has a null pointer to value"};
        }
    }
}

std::tuple<table_ptr, Limits> allocate_table(
    const std::vector<Table>& module_tables, const std::vector<ExternalTable>& imported_tables)
{
    static const auto table_delete = [](table_elements* t) noexcept { delete t; };
    static const auto null_delete = [](table_elements*) noexcept {};

    assert(module_tables.size() + imported_tables.size() <= 1);

    if (module_tables.size() == 1)
    {
        return {table_ptr{new table_elements(module_tables[0].limits.min), table_delete},
            module_tables[0].limits};
    }
    else if (imported_tables.size() == 1)
        return {table_ptr{imported_tables[0].table, null_delete}, imported_tables[0].limits};
    else
        return {table_ptr{nullptr, null_delete}, Limits{}};
}

std::tuple<bytes_ptr, Limits> allocate_memory(const std::vector<Memory>& module_memories,
    const std::vector<ExternalMemory>& imported_memories, uint32_t memory_pages_limit)
{
    static const auto bytes_delete = [](bytes* b) noexcept { delete b; };
    static const auto null_delete = [](bytes*) noexcept {};

    assert(module_memories.size() + imported_memories.size() <= 1);

    if (module_memories.size() == 1)
    {
        const auto memory_min = module_memories[0].limits.min;
        const auto memory_max = module_memories[0].limits.max;

        // TODO: better error handling
        if ((memory_min > memory_pages_limit) ||
            (memory_max.has_value() && *memory_max > memory_pages_limit))
        {
            throw instantiate_error{"cannot exceed hard memory limit of " +
                                    std::to_string(memory_pages_limit * PageSize) + " bytes"};
        }

        // NOTE: fill it with zeroes
        bytes_ptr memory{new bytes(memory_min * PageSize, 0), bytes_delete};
        return {std::move(memory), module_memories[0].limits};
    }
    else if (imported_memories.size() == 1)
    {
        const auto memory_min = imported_memories[0].limits.min;
        const auto memory_max = imported_memories[0].limits.max;

        // TODO: better error handling
        if ((memory_min > memory_pages_limit) ||
            (memory_max.has_value() && *memory_max > memory_pages_limit))
        {
            throw instantiate_error{"imported memory limits cannot exceed hard memory limit of " +
                                    std::to_string(memory_pages_limit * PageSize) + " bytes"};
        }

        bytes_ptr memory{imported_memories[0].data, null_delete};
        return {std::move(memory), imported_memories[0].limits};
    }
    else
    {
        bytes_ptr memory{nullptr, null_delete};
        return {std::move(memory), Limits{}};
    }
}

Value eval_constant_expression(ConstantExpression expr,
    const std::vector<ExternalGlobal>& imported_globals, const std::vector<Value>& globals)
{
    if (expr.kind == ConstantExpression::Kind::Constant)
        return expr.value.constant;

    assert(expr.kind == ConstantExpression::Kind::GlobalGet);

    const auto global_idx = expr.value.global_index;
    assert(global_idx < imported_globals.size() + globals.size());

    if (global_idx < imported_globals.size())
        return *imported_globals[global_idx].value;
    else
        return globals[global_idx - imported_globals.size()];
}

std::optional<uint32_t> find_export(const Module& module, ExternalKind kind, std::string_view name)
{
    const auto it = std::find_if(module.exportsec.begin(), module.exportsec.end(),
        [kind, name](const auto& export_) { return export_.kind == kind && export_.name == name; });

    return (it != module.exportsec.end() ? std::make_optional(it->index) : std::nullopt);
}

}  // namespace

std::unique_ptr<Instance> instantiate(std::unique_ptr<const Module> module,
    std::vector<ExternalFunction> imported_functions, std::vector<ExternalTable> imported_tables,
    std::vector<ExternalMemory> imported_memories, std::vector<ExternalGlobal> imported_globals,
    uint32_t memory_pages_limit /*= DefaultMemoryPagesLimit*/)
{
    assert(module->funcsec.size() == module->codesec.size());

    match_imported_functions(module->imported_function_types, imported_functions);
    match_imported_tables(module->imported_table_types, imported_tables);
    match_imported_memories(module->imported_memory_types, imported_memories);
    match_imported_globals(module->imported_global_types, imported_globals);

    // Init globals
    std::vector<Value> globals;
    globals.reserve(module->globalsec.size());
    for (auto const& global : module->globalsec)
    {
        // Constraint to use global.get only with imported globals is checked at validation.
        assert(global.expression.kind != ConstantExpression::Kind::GlobalGet ||
               global.expression.value.global_index < imported_globals.size());

        const auto value = eval_constant_expression(global.expression, imported_globals, globals);
        globals.emplace_back(value);
    }

    auto [table, table_limits] = allocate_table(module->tablesec, imported_tables);

    auto [memory, memory_limits] =
        allocate_memory(module->memorysec, imported_memories, memory_pages_limit);
    // In case upper limit for local/imported memory is defined,
    // we adjust the hard memory limit, to ensure memory.grow will fail when exceeding it.
    // Note: allocate_memory ensures memory's max limit is always below memory_pages_limit.
    if (memory_limits.max.has_value())
    {
        assert(*memory_limits.max <= memory_pages_limit);
        memory_pages_limit = *memory_limits.max;
    }

    // Before starting to fill memory and table,
    // check that data and element segments are within bounds.
    std::vector<uint64_t> datasec_offsets;
    datasec_offsets.reserve(module->datasec.size());
    for (const auto& data : module->datasec)
    {
        // Offset is validated to be i32, but it's used in 64-bit calculation below.
        const uint64_t offset =
            eval_constant_expression(data.offset, imported_globals, globals).i64;

        if (offset + data.init.size() > memory->size())
            throw instantiate_error{"data segment is out of memory bounds"};

        datasec_offsets.emplace_back(offset);
    }

    assert(module->elementsec.empty() || table != nullptr);
    std::vector<ptrdiff_t> elementsec_offsets;
    elementsec_offsets.reserve(module->elementsec.size());
    for (const auto& element : module->elementsec)
    {
        // Offset is validated to be i32, but it's used in 64-bit calculation below.
        const uint64_t offset =
            eval_constant_expression(element.offset, imported_globals, globals).i64;

        if (offset + element.init.size() > table->size())
            throw instantiate_error{"element segment is out of table bounds"};

        elementsec_offsets.emplace_back(offset);
    }

    // Fill out memory based on data segments
    for (size_t i = 0; i < module->datasec.size(); ++i)
    {
        // NOTE: these instructions can overlap
        std::copy(module->datasec[i].init.begin(), module->datasec[i].init.end(),
            memory->data() + datasec_offsets[i]);
    }

    // We need to create instance before filling table,
    // because table functions will capture the pointer to instance.
    auto instance = std::make_unique<Instance>(std::move(module), std::move(memory), memory_limits,
        // TODO: Clang Analyzer reports 3 potential memory leaks in std::move(memory),
        //       std::move(table), and std::move(globals). Report bug if false positive.
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks)
        memory_pages_limit, std::move(table), table_limits, std::move(globals),
        std::move(imported_functions), std::move(imported_globals));

    // Fill the table based on elements segment
    for (size_t i = 0; i < instance->module->elementsec.size(); ++i)
    {
        // Overwrite table[offset..] with element.init
        auto it_table = instance->table->begin() + elementsec_offsets[i];
        for (const auto idx : instance->module->elementsec[i].init)
        {
            *it_table++ = {instance.get(), idx, {}};
        }
    }

    // Run start function if present
    if (instance->module->startfunc)
    {
        const auto funcidx = *instance->module->startfunc;
        assert(funcidx < instance->imported_functions.size() + instance->module->funcsec.size());
        if (execute(*instance, funcidx, {}).trapped)
        {
            // When element section modified imported table, and then start function trapped,
            // modifications to the table are not rolled back.
            // Instance in this case is not being returned to the user, so it needs to be kept alive
            // as long as functions using it are alive in the table.
            if (!imported_tables.empty() && !instance->module->elementsec.empty())
            {
                // Instance may be used by several functions added to the table,
                // so we need a shared ownership here.
                std::shared_ptr<Instance> shared_instance = std::move(instance);

                for (size_t i = 0; i < shared_instance->module->elementsec.size(); ++i)
                {
                    auto it_table = shared_instance->table->begin() + elementsec_offsets[i];
                    for ([[maybe_unused]] auto _ : shared_instance->module->elementsec[i].init)
                    {
                        // Capture shared instance in table element.
                        (*it_table++).shared_instance = shared_instance;
                    }
                }
            }
            throw instantiate_error{"start function failed to execute"};
        }
    }

    return instance;
}

std::vector<ExternalFunction> resolve_imported_functions(
    const Module& module, std::vector<ImportedFunction> imported_functions)
{
    std::vector<ExternalFunction> external_functions;
    for (const auto& import : module.importsec)
    {
        if (import.kind != ExternalKind::Function)
            continue;

        const auto it = std::find_if(
            imported_functions.begin(), imported_functions.end(), [&import](const auto& func) {
                return import.module == func.module && import.name == func.name;
            });

        if (it == imported_functions.end())
        {
            throw instantiate_error{
                "imported function " + import.module + "." + import.name + " is required"};
        }

        assert(import.desc.function_type_index < module.typesec.size());
        const auto& module_func_type = module.typesec[import.desc.function_type_index];

        if (module_func_type.inputs != it->inputs)
        {
            throw instantiate_error{"function " + import.module + "." + import.name +
                                    " input types don't match imported function in module"};
        }
        if (module_func_type.outputs.empty() && it->output.has_value())
        {
            throw instantiate_error{"function " + import.module + "." + import.name +
                                    " has output but is defined void in module"};
        }
        if (!module_func_type.outputs.empty() &&
            (!it->output.has_value() || module_func_type.outputs[0] != *it->output))
        {
            throw instantiate_error{"function " + import.module + "." + import.name +
                                    " output type doesn't match imported function in module"};
        }

        external_functions.emplace_back(
            ExternalFunction{it->function, it->context, module_func_type});
    }

    return external_functions;
}

std::optional<FuncIdx> find_exported_function(const Module& module, std::string_view name)
{
    return find_export(module, ExternalKind::Function, name);
}

std::optional<ExportedFunction> find_exported_function(Instance& instance, std::string_view name)
{
    const auto opt_index = find_export(*instance.module, ExternalKind::Function, name);
    if (!opt_index.has_value())
        return std::nullopt;

    const auto idx = *opt_index;
    auto func = [](void* context, fizzy::Instance&, const Value* args, int depth) {
        auto* instance_and_idx = static_cast<std::pair<Instance*, FuncIdx>*>(context);
        return execute(*instance_and_idx->first, instance_and_idx->second, args, depth);
    };

    ExportedFunction exported_func;
    exported_func.context.reset(new std::pair<Instance*, FuncIdx>(&instance, idx));
    exported_func.external_function = {
        std::move(func), exported_func.context.get(), instance.module->get_function_type(idx)};
    return exported_func;
}

std::optional<ExternalGlobal> find_exported_global(Instance& instance, std::string_view name)
{
    const auto opt_index = find_export(*instance.module, ExternalKind::Global, name);
    if (!opt_index.has_value())
        return std::nullopt;

    const auto global_idx = *opt_index;
    if (global_idx < instance.imported_globals.size())
    {
        // imported global is reexported
        return ExternalGlobal{instance.imported_globals[global_idx].value,
            instance.imported_globals[global_idx].type};
    }
    else
    {
        // global owned by instance
        const auto module_global_idx = global_idx - instance.imported_globals.size();
        return ExternalGlobal{&instance.globals[module_global_idx],
            instance.module->globalsec[module_global_idx].type};
    }
}

std::optional<ExternalTable> find_exported_table(Instance& instance, std::string_view name)
{
    // Index returned from find_export is discarded, because there's no more than 1 table
    if (!find_export(*instance.module, ExternalKind::Table, name))
        return std::nullopt;

    return ExternalTable{instance.table.get(), instance.table_limits};
}

std::optional<ExternalMemory> find_exported_memory(Instance& instance, std::string_view name)
{
    // Index returned from find_export is discarded, because there's no more than 1 memory
    if (!find_export(*instance.module, ExternalKind::Memory, name))
        return std::nullopt;

    return ExternalMemory{instance.memory.get(), instance.memory_limits};
}

}  // namespace fizzy
