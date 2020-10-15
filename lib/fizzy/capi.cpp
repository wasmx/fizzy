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

inline const FizzyValue* wrap(const fizzy::Value* values) noexcept
{
    return reinterpret_cast<const FizzyValue*>(values);
}

inline const fizzy::Value* unwrap(const FizzyValue* values) noexcept
{
    return reinterpret_cast<const fizzy::Value*>(values);
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

inline auto unwrap(FizzyExternalFn func, void* context) noexcept
{
    return [func, context](fizzy::Instance& instance, const fizzy::Value* args,
               int depth) noexcept -> fizzy::ExecutionResult {
        const auto result = func(context, wrap(&instance), wrap(args), depth);
        return unwrap(result);
    };
}

inline fizzy::ExternalFunction unwrap(const FizzyExternalFunction& external_func)
{
    return fizzy::ExternalFunction{
        unwrap(external_func.function, external_func.context), unwrap(external_func.type)};
}

inline fizzy::ImportedFunction unwrap(const FizzyImportedFunction& c_imported_func)
{
    fizzy::ImportedFunction imported_func;
    imported_func.module =
        c_imported_func.module ? std::string{c_imported_func.module} : std::string{};
    imported_func.name = c_imported_func.name ? std::string{c_imported_func.name} : std::string{};

    const auto& c_type = c_imported_func.external_function.type;
    fizzy::ValType (*unwrap_valtype_fn)(FizzyValueType value) = &unwrap;
    std::transform(c_type.inputs, c_type.inputs + c_type.inputs_size, imported_func.inputs.begin(),
        unwrap_valtype_fn);
    imported_func.output = c_type.output == FizzyValueTypeVoid ?
                               std::nullopt :
                               std::make_optional(unwrap(c_type.output));

    imported_func.function = unwrap(
        c_imported_func.external_function.function, c_imported_func.external_function.context);

    return imported_func;
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

FizzyInstance* fizzy_instantiate(const FizzyModule* module,
    const FizzyExternalFunction* imported_functions, size_t imported_functions_size)
{
    try
    {
        std::vector<fizzy::ExternalFunction> functions(imported_functions_size);
        fizzy::ExternalFunction (*unwrap_external_func_fn)(const FizzyExternalFunction&) = &unwrap;
        std::transform(imported_functions, imported_functions + imported_functions_size,
            functions.begin(), unwrap_external_func_fn);

        auto instance = fizzy::instantiate(
            std::unique_ptr<const fizzy::Module>(unwrap(module)), std::move(functions));

        return wrap(instance.release());
    }
    catch (...)
    {
        return nullptr;
    }
}

FizzyInstance* fizzy_resolve_instantiate(const FizzyModule* c_module,
    const FizzyImportedFunction* c_imported_functions, size_t imported_functions_size)
{
    try
    {
        std::vector<fizzy::ImportedFunction> imported_functions(imported_functions_size);
        fizzy::ImportedFunction (*unwrap_imported_func_fn)(const FizzyImportedFunction&) = &unwrap;
        std::transform(c_imported_functions, c_imported_functions + imported_functions_size,
            imported_functions.begin(), unwrap_imported_func_fn);

        std::unique_ptr<const fizzy::Module> module{unwrap(c_module)};
        auto resolved_imports = fizzy::resolve_imported_functions(*module, imported_functions);

        auto instance = fizzy::instantiate(std::move(module), std::move(resolved_imports));

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
