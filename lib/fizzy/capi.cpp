// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "cxx20/bit.hpp"
#include "execute.hpp"
#include "instantiate.hpp"
#include "parser.hpp"
#include <fizzy/fizzy.h>
#include <memory>

namespace
{
inline const FizzyModule* wrap(const fizzy::Module* module) noexcept
{
    return reinterpret_cast<const FizzyModule*>(module);
}

inline const fizzy::Module* unwrap(const FizzyModule* module) noexcept
{
    return reinterpret_cast<const fizzy::Module*>(module);
}

inline const FizzyValueType* wrap(const fizzy::ValType* value_types) noexcept
{
    static_assert(sizeof(FizzyValueType) == sizeof(fizzy::ValType));
    return reinterpret_cast<const FizzyValueType*>(value_types);
}

inline FizzyValueType wrap(fizzy::ValType value_type) noexcept
{
    return static_cast<FizzyValueType>(value_type);
}

inline fizzy::ValType unwrap(FizzyValueType value_type) noexcept
{
    return static_cast<fizzy::ValType>(value_type);
}

inline FizzyFunctionType wrap(const fizzy::FuncType& type) noexcept
{
    return {(type.outputs.empty() ? FizzyValueTypeVoid : wrap(type.outputs[0])),
        (type.inputs.empty() ? nullptr : wrap(&type.inputs[0])), type.inputs.size()};
}

inline fizzy::FuncType unwrap(const FizzyFunctionType& type)
{
    fizzy::FuncType func_type{std::vector<fizzy::ValType>(type.inputs_size),
        (type.output == FizzyValueTypeVoid ? std::vector<fizzy::ValType>{} :
                                             std::vector<fizzy::ValType>{unwrap(type.output)})};

    fizzy::ValType (*unwrap_valtype_fn)(FizzyValueType value) = &unwrap;
    std::transform(
        type.inputs, type.inputs + type.inputs_size, func_type.inputs.begin(), unwrap_valtype_fn);

    return func_type;
}

inline FizzyValue wrap(fizzy::Value value) noexcept
{
    return fizzy::bit_cast<FizzyValue>(value);
}

inline fizzy::Value unwrap(FizzyValue value) noexcept
{
    return fizzy::bit_cast<fizzy::Value>(value);
}

inline FizzyValue* wrap(fizzy::Value* values) noexcept
{
    return reinterpret_cast<FizzyValue*>(values);
}

inline const FizzyValue* wrap(const fizzy::Value* values) noexcept
{
    return reinterpret_cast<const FizzyValue*>(values);
}

inline const fizzy::Value* unwrap(const FizzyValue* values) noexcept
{
    return reinterpret_cast<const fizzy::Value*>(values);
}

inline fizzy::Value* unwrap(FizzyValue* value) noexcept
{
    return reinterpret_cast<fizzy::Value*>(value);
}

inline FizzyInstance* wrap(fizzy::Instance* instance) noexcept
{
    return reinterpret_cast<FizzyInstance*>(instance);
}

inline fizzy::Instance* unwrap(FizzyInstance* instance) noexcept
{
    return reinterpret_cast<fizzy::Instance*>(instance);
}

inline FizzyExecutionResult wrap(const fizzy::ExecutionResult& result) noexcept
{
    return {result.trapped, result.has_value, wrap(result.value)};
}

inline fizzy::ExecutionResult unwrap(const FizzyExecutionResult& result) noexcept
{
    if (result.trapped)
        return fizzy::Trap;
    else if (!result.has_value)
        return fizzy::Void;
    else
        return unwrap(result.value);
}

fizzy::ExecutionResult unwrapped_external_function(
    void* context, fizzy::Instance& instance, const fizzy::Value* args, int depth)
{
    const auto* c_external_function = static_cast<const FizzyExternalFunction*>(context);
    return unwrap(c_external_function->function(
        c_external_function->context, wrap(&instance), wrap(args), depth));
}


inline fizzy::ExternalFunction unwrap(const FizzyExternalFunction& external_func)
{
    return fizzy::ExternalFunction{unwrapped_external_function,
        const_cast<FizzyExternalFunction*>(&external_func), unwrap(external_func.type)};
}

inline std::vector<fizzy::ExternalFunction> unwrap(
    const FizzyExternalFunction* c_external_functions, size_t external_functions_size)
{
    std::vector<fizzy::ExternalFunction> external_functions(external_functions_size);
    fizzy::ExternalFunction (*unwrap_external_func_fn)(const FizzyExternalFunction&) = &unwrap;
    std::transform(c_external_functions, c_external_functions + external_functions_size,
        external_functions.begin(), unwrap_external_func_fn);
    return external_functions;
}

inline fizzy::ImportedFunction unwrap(const FizzyImportedFunction& c_imported_func)
{
    fizzy::ImportedFunction imported_func;
    imported_func.module =
        c_imported_func.module ? std::string{c_imported_func.module} : std::string{};
    imported_func.name = c_imported_func.name ? std::string{c_imported_func.name} : std::string{};

    const auto& c_type = c_imported_func.external_function.type;
    imported_func.inputs.resize(c_type.inputs_size);
    fizzy::ValType (*unwrap_valtype_fn)(FizzyValueType value) = &unwrap;
    std::transform(c_type.inputs, c_type.inputs + c_type.inputs_size, imported_func.inputs.begin(),
        unwrap_valtype_fn);
    imported_func.output = c_type.output == FizzyValueTypeVoid ?
                               std::nullopt :
                               std::make_optional(unwrap(c_type.output));

    imported_func.function = unwrapped_external_function;
    imported_func.context = const_cast<FizzyExternalFunction*>(&c_imported_func.external_function);

    return imported_func;
}

inline std::vector<fizzy::ImportedFunction> unwrap(
    const FizzyImportedFunction* c_imported_functions, size_t imported_functions_size)
{
    std::vector<fizzy::ImportedFunction> imported_functions(imported_functions_size);
    fizzy::ImportedFunction (*unwrap_imported_func_fn)(const FizzyImportedFunction&) = &unwrap;
    std::transform(c_imported_functions, c_imported_functions + imported_functions_size,
        imported_functions.begin(), unwrap_imported_func_fn);
    return imported_functions;
}

inline FizzyTable* wrap(fizzy::table_elements* table) noexcept
{
    return reinterpret_cast<FizzyTable*>(table);
}

inline fizzy::table_elements* unwrap(FizzyTable* table) noexcept
{
    return reinterpret_cast<fizzy::table_elements*>(table);
}

inline FizzyMemory* wrap(fizzy::bytes* memory) noexcept
{
    return reinterpret_cast<FizzyMemory*>(memory);
}

inline fizzy::bytes* unwrap(FizzyMemory* memory) noexcept
{
    return reinterpret_cast<fizzy::bytes*>(memory);
}

inline FizzyLimits wrap(const fizzy::Limits& limits) noexcept
{
    return {limits.min, (limits.max.has_value() ? *limits.max : 0), limits.max.has_value()};
}

inline fizzy::Limits unwrap(const FizzyLimits& limits) noexcept
{
    return {limits.min, limits.has_max ? std::make_optional(limits.max) : std::nullopt};
}

inline FizzyExternalTable wrap(const fizzy::ExternalTable& external_table) noexcept
{
    return {wrap(external_table.table), wrap(external_table.limits)};
}

inline fizzy::ExternalTable unwrap(const FizzyExternalTable& external_table) noexcept
{
    return {unwrap(external_table.table), unwrap(external_table.limits)};
}

inline FizzyExternalMemory wrap(const fizzy::ExternalMemory& external_memory) noexcept
{
    return {wrap(external_memory.data), wrap(external_memory.limits)};
}

inline fizzy::ExternalMemory unwrap(const FizzyExternalMemory& external_memory) noexcept
{
    return {unwrap(external_memory.memory), unwrap(external_memory.limits)};
}

inline std::vector<fizzy::ExternalTable> unwrap(const FizzyExternalTable* external_table)
{
    return external_table ? std::vector<fizzy::ExternalTable>{unwrap(*external_table)} :
                            std::vector<fizzy::ExternalTable>{};
}

inline std::vector<fizzy::ExternalMemory> unwrap(const FizzyExternalMemory* external_memory)
{
    return external_memory ? std::vector<fizzy::ExternalMemory>{unwrap(*external_memory)} :
                             std::vector<fizzy::ExternalMemory>{};
}

inline FizzyGlobalType wrap(const fizzy::GlobalType& global_type) noexcept
{
    return {wrap(global_type.value_type), global_type.is_mutable};
}

inline fizzy::GlobalType unwrap(const FizzyGlobalType& global_type) noexcept
{
    return {unwrap(global_type.value_type), global_type.is_mutable};
}

inline FizzyExternalGlobal wrap(const fizzy::ExternalGlobal& external_global) noexcept
{
    return {wrap(external_global.value), wrap(external_global.type)};
}

inline fizzy::ExternalGlobal unwrap(const FizzyExternalGlobal& external_global) noexcept
{
    return {unwrap(external_global.value), unwrap(external_global.type)};
}

inline std::vector<fizzy::ExternalGlobal> unwrap(
    const FizzyExternalGlobal* c_external_globals, size_t external_globals_size)
{
    std::vector<fizzy::ExternalGlobal> external_globals(external_globals_size);
    fizzy::ExternalGlobal (*unwrap_external_global_fn)(const FizzyExternalGlobal&) = &unwrap;
    std::transform(c_external_globals, c_external_globals + external_globals_size,
        external_globals.begin(), unwrap_external_global_fn);
    return external_globals;
}
}  // namespace

extern "C" {
bool fizzy_validate(const uint8_t* wasm_binary, size_t wasm_binary_size)
{
    try
    {
        fizzy::parse({wasm_binary, wasm_binary_size});
        return true;
    }
    catch (...)
    {
        return false;
    }
}

const FizzyModule* fizzy_parse(const uint8_t* wasm_binary, size_t wasm_binary_size)
{
    try
    {
        auto module = fizzy::parse({wasm_binary, wasm_binary_size});
        return wrap(module.release());
    }
    catch (...)
    {
        return nullptr;
    }
}

void fizzy_free_module(const FizzyModule* module)
{
    delete unwrap(module);
}

FizzyFunctionType fizzy_get_function_type(const FizzyModule* module, uint32_t func_idx)
{
    return wrap(unwrap(module)->get_function_type(func_idx));
}

bool fizzy_find_exported_function(
    const FizzyModule* module, const char* name, uint32_t* out_func_idx)
{
    const auto optional_func_idx = fizzy::find_exported_function(*unwrap(module), name);
    if (!optional_func_idx)
        return false;

    *out_func_idx = *optional_func_idx;
    return true;
}

bool fizzy_find_exported_table(
    FizzyInstance* instance, const char* name, FizzyExternalTable* out_table)
{
    const auto optional_external_table = fizzy::find_exported_table(*unwrap(instance), name);
    if (!optional_external_table)
        return false;

    *out_table = wrap(*optional_external_table);
    return true;
}

bool fizzy_find_exported_memory(
    FizzyInstance* instance, const char* name, FizzyExternalMemory* out_memory)
{
    const auto optional_external_memory = fizzy::find_exported_memory(*unwrap(instance), name);
    if (!optional_external_memory)
        return false;

    *out_memory = wrap(*optional_external_memory);
    return true;
}

bool fizzy_find_exported_global(
    FizzyInstance* instance, const char* name, FizzyExternalGlobal* out_global)
{
    const auto optional_external_global = fizzy::find_exported_global(*unwrap(instance), name);
    if (!optional_external_global)
        return false;

    *out_global = wrap(*optional_external_global);
    return true;
}

FizzyInstance* fizzy_instantiate(const FizzyModule* module,
    const FizzyExternalFunction* imported_functions, size_t imported_functions_size,
    const FizzyExternalTable* imported_table, const FizzyExternalMemory* imported_memory,
    const FizzyExternalGlobal* imported_globals, size_t imported_globals_size)
{
    try
    {
        auto functions = unwrap(imported_functions, imported_functions_size);
        auto table = unwrap(imported_table);
        auto memory = unwrap(imported_memory);
        auto globals = unwrap(imported_globals, imported_globals_size);

        auto instance = fizzy::instantiate(std::unique_ptr<const fizzy::Module>(unwrap(module)),
            std::move(functions), std::move(table), std::move(memory), std::move(globals));

        return wrap(instance.release());
    }
    catch (...)
    {
        return nullptr;
    }
}

FizzyInstance* fizzy_resolve_instantiate(const FizzyModule* c_module,
    const FizzyImportedFunction* c_imported_functions, size_t imported_functions_size,
    const FizzyExternalTable* imported_table, const FizzyExternalMemory* imported_memory,
    const FizzyExternalGlobal* imported_globals, size_t imported_globals_size)
{
    try
    {
        auto imported_functions = unwrap(c_imported_functions, imported_functions_size);
        auto table = unwrap(imported_table);
        auto memory = unwrap(imported_memory);
        auto globals = unwrap(imported_globals, imported_globals_size);

        std::unique_ptr<const fizzy::Module> module{unwrap(c_module)};
        auto resolved_imports = fizzy::resolve_imported_functions(*module, imported_functions);

        auto instance = fizzy::instantiate(std::move(module), std::move(resolved_imports),
            std::move(table), std::move(memory), std::move(globals));

        return wrap(instance.release());
    }
    catch (...)
    {
        return nullptr;
    }
}

void fizzy_free_instance(FizzyInstance* instance)
{
    delete unwrap(instance);
}

const FizzyModule* fizzy_get_instance_module(FizzyInstance* instance)
{
    return wrap(unwrap(instance)->module.get());
}

uint8_t* fizzy_get_instance_memory_data(FizzyInstance* instance)
{
    auto& memory = unwrap(instance)->memory;
    if (!memory)
        return nullptr;

    return memory->data();
}

size_t fizzy_get_instance_memory_size(FizzyInstance* instance)
{
    auto& memory = unwrap(instance)->memory;
    if (!memory)
        return 0;

    return memory->size();
}

FizzyExecutionResult fizzy_execute(
    FizzyInstance* instance, uint32_t func_idx, const FizzyValue* args, int depth)
{
    const auto result = fizzy::execute(*unwrap(instance), func_idx, unwrap(args), depth);
    return wrap(result);
}
}
