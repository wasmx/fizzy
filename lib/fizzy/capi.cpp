// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "cxx20/bit.hpp"
#include "cxx23/utility.hpp"
#include "execute.hpp"
#include "instantiate.hpp"
#include "parser.hpp"
#include <fizzy/fizzy.h>
#include <cstring>
#include <memory>

namespace
{
inline void set_success(FizzyError* error) noexcept
{
    if (error == nullptr)
        return;

    error->code = FizzySuccess;
    error->message[0] = '\0';
}

// Copying a string into a fixed-size static buffer, guaranteed to not overrun it and to always end
// the destination string with a null terminator. Inspired by strlcpy.
template <size_t N>
inline size_t truncating_strlcpy(char (&dest)[N], const char* src) noexcept
{
    static_assert(N >= 4);

    const auto src_len = strlen(src);
    const auto copy_len = std::min(src_len, N - 1);
    memcpy(dest, src, copy_len);
    if (copy_len < src_len)
    {
        dest[copy_len - 3] = '.';
        dest[copy_len - 2] = '.';
        dest[copy_len - 1] = '.';
    }
    dest[copy_len] = '\0';
    return copy_len;
}

inline void set_error_code_and_message(
    FizzyErrorCode code, const char* message, FizzyError* error) noexcept
{
    error->code = code;
    truncating_strlcpy(error->message, message);
}

inline void set_error_from_current_exception(FizzyError* error) noexcept
{
    if (error == nullptr)
        return;

    try
    {
        throw;
    }
    catch (const fizzy::parser_error& e)
    {
        set_error_code_and_message(FizzyErrorMalformedModule, e.what(), error);
    }
    catch (const fizzy::validation_error& e)
    {
        set_error_code_and_message(FizzyErrorInvalidModule, e.what(), error);
    }
    catch (const fizzy::instantiate_error& e)
    {
        set_error_code_and_message(FizzyErrorInstantiationFailed, e.what(), error);
    }
    catch (const std::bad_alloc&)
    {
        set_error_code_and_message(
            FizzyErrorMemoryAllocationFailed, "memory allocation failed", error);
    }
    catch (const std::exception& e)
    {
        set_error_code_and_message(FizzyErrorOther, e.what(), error);
    }
    catch (...)
    {
        set_error_code_and_message(FizzyErrorOther, "unknown error", error);
    }
}

inline const FizzyModule* wrap(const fizzy::Module* module) noexcept
{
    return reinterpret_cast<const FizzyModule*>(module);
}

inline const fizzy::Module* unwrap(const FizzyModule* module) noexcept
{
    return reinterpret_cast<const fizzy::Module*>(module);
}

static_assert(sizeof(FizzyValueType) == sizeof(fizzy::ValType));
static_assert(FizzyValueTypeI32 == fizzy::to_underlying(fizzy::ValType::i32));
static_assert(FizzyValueTypeI64 == fizzy::to_underlying(fizzy::ValType::i64));
static_assert(FizzyValueTypeF32 == fizzy::to_underlying(fizzy::ValType::f32));
static_assert(FizzyValueTypeF64 == fizzy::to_underlying(fizzy::ValType::f64));
static_assert(FizzyValueTypeVoid == 0);
static_assert(std::is_same_v<decltype(fizzy::to_underlying(fizzy::ValType::i32)),
    std::remove_const<decltype(FizzyValueTypeI32)>::type>);

inline const FizzyValueType* wrap(const fizzy::ValType* value_types) noexcept
{
    return reinterpret_cast<const FizzyValueType*>(value_types);
}

inline const fizzy::ValType* unwrap(const FizzyValueType* c_value_types) noexcept
{
    return reinterpret_cast<const fizzy::ValType*>(c_value_types);
}

inline FizzyValueType wrap(fizzy::ValType value_type) noexcept
{
    return static_cast<FizzyValueType>(value_type);
}

inline fizzy::ValType unwrap(FizzyValueType value_type) noexcept
{
    return static_cast<fizzy::ValType>(value_type);
}

inline FizzyFunctionType wrap(fizzy::span<const fizzy::ValType> input_types,
    fizzy::span<const fizzy::ValType> output_types) noexcept
{
    return {(output_types.empty() ? FizzyValueTypeVoid : wrap(output_types[0])),
        (input_types.empty() ? nullptr : wrap(&input_types[0])), input_types.size()};
}

inline FizzyFunctionType wrap(const fizzy::FuncType& type) noexcept
{
    return wrap(type.inputs, type.outputs);
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

inline FizzyExecutionContext* wrap(fizzy::ExecutionContext* ctx) noexcept
{
    return reinterpret_cast<FizzyExecutionContext*>(ctx);
}

inline fizzy::ExecutionContext* unwrap(FizzyExecutionContext* ctx) noexcept
{
    return reinterpret_cast<fizzy::ExecutionContext*>(ctx);
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

inline fizzy::ExecuteFunction unwrap(FizzyExternalFn c_function, void* c_host_context)
{
    static constexpr fizzy::HostFunctionPtr function =
        [](std::any& host_ctx, fizzy::Instance& instance, const fizzy::Value* args,
            fizzy::ExecutionContext& ctx) noexcept {
            const auto [c_func, c_host_ctx] =
                *std::any_cast<std::pair<FizzyExternalFn, void*>>(&host_ctx);
            return unwrap(c_func(c_host_ctx, wrap(&instance), wrap(args), wrap(&ctx)));
        };

    return {function, std::make_any<std::pair<FizzyExternalFn, void*>>(c_function, c_host_context)};
}

inline FizzyExternalFunction wrap(fizzy::ExternalFunction external_func)
{
    static constexpr FizzyExternalFn c_function =
        [](void* host_ctx, FizzyInstance* instance, const FizzyValue* args,
            FizzyExecutionContext* c_ctx) noexcept -> FizzyExecutionResult {
        // If execution context not provided, allocate new one.
        // It must be allocated on heap otherwise the stack will explode in recursive calls.
        std::unique_ptr<fizzy::ExecutionContext> new_ctx;
        if (c_ctx == nullptr)
            new_ctx = std::make_unique<fizzy::ExecutionContext>();
        auto* ctx = new_ctx ? new_ctx.get() : unwrap(c_ctx);

        auto* func = static_cast<fizzy::ExternalFunction*>(host_ctx);
        return wrap((func->function)(*unwrap(instance), unwrap(args), *ctx));
    };

    auto host_ctx = std::make_unique<fizzy::ExternalFunction>(std::move(external_func));
    const auto c_type = wrap(host_ctx->input_types, host_ctx->output_types);
    void* c_host_ctx = host_ctx.release();
    return {c_type, c_function, c_host_ctx};
}

inline fizzy::ExternalFunction unwrap(const FizzyExternalFunction& external_func)
{
    const fizzy::span<const fizzy::ValType> input_types{
        unwrap(external_func.type.inputs), external_func.type.inputs_size};
    const fizzy::span<const fizzy::ValType> output_types =
        (external_func.type.output == FizzyValueTypeVoid ?
                fizzy::span<const fizzy::ValType>{} :
                fizzy::span<const fizzy::ValType>{unwrap(&external_func.type.output), 1});

    return fizzy::ExternalFunction{
        unwrap(external_func.function, external_func.context), input_types, output_types};
}

inline std::vector<fizzy::ExternalFunction> unwrap(
    const FizzyExternalFunction* c_external_functions, size_t external_functions_size)
{
    std::vector<fizzy::ExternalFunction> external_functions;
    external_functions.reserve(external_functions_size);
    fizzy::ExternalFunction (*unwrap_external_func_fn)(const FizzyExternalFunction&) = &unwrap;
    std::transform(c_external_functions, c_external_functions + external_functions_size,
        std::back_inserter(external_functions), unwrap_external_func_fn);
    return external_functions;
}

inline fizzy::ImportedFunction unwrap(const FizzyImportedFunction& c_imported_func)
{
    const auto& c_type = c_imported_func.external_function.type;

    std::vector<fizzy::ValType> inputs(c_type.inputs_size);
    fizzy::ValType (*unwrap_valtype_fn)(FizzyValueType value) = &unwrap;
    std::transform(
        c_type.inputs, c_type.inputs + c_type.inputs_size, inputs.begin(), unwrap_valtype_fn);

    const std::optional<fizzy::ValType> output(c_type.output == FizzyValueTypeVoid ?
                                                   std::nullopt :
                                                   std::make_optional(unwrap(c_type.output)));

    auto function = unwrap(
        c_imported_func.external_function.function, c_imported_func.external_function.context);

    return {c_imported_func.module, c_imported_func.name, std::move(inputs), output,
        std::move(function)};
}

inline std::vector<fizzy::ImportedFunction> unwrap(
    const FizzyImportedFunction* c_imported_functions, size_t imported_functions_size)
{
    std::vector<fizzy::ImportedFunction> imported_functions;
    imported_functions.reserve(imported_functions_size);

    fizzy::ImportedFunction (*unwrap_imported_func_fn)(const FizzyImportedFunction&) = &unwrap;
    std::transform(c_imported_functions, c_imported_functions + imported_functions_size,
        std::back_inserter(imported_functions), unwrap_imported_func_fn);

    return imported_functions;
}

inline fizzy::ImportedGlobal unwrap(const FizzyImportedGlobal& c_imported_global)
{
    return {c_imported_global.module, c_imported_global.name,
        unwrap(c_imported_global.external_global.value),
        unwrap(c_imported_global.external_global.type.value_type),
        c_imported_global.external_global.type.is_mutable};
}

inline std::vector<fizzy::ImportedGlobal> unwrap(
    const FizzyImportedGlobal* c_imported_globals, size_t imported_globals_size)
{
    std::vector<fizzy::ImportedGlobal> imported_globals(imported_globals_size);
    fizzy::ImportedGlobal (*unwrap_imported_global_fn)(const FizzyImportedGlobal&) = &unwrap;
    std::transform(c_imported_globals, c_imported_globals + imported_globals_size,
        imported_globals.begin(), unwrap_imported_global_fn);
    return imported_globals;
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

inline FizzyExternalKind wrap(fizzy::ExternalKind kind) noexcept
{
    return static_cast<FizzyExternalKind>(kind);
}

inline FizzyImportDescription wrap(
    const fizzy::Import& import, const fizzy::Module& module) noexcept
{
    FizzyImportDescription c_import_description;
    c_import_description.module = import.module.c_str();
    c_import_description.name = import.name.c_str();
    c_import_description.kind = wrap(import.kind);
    switch (c_import_description.kind)
    {
    case FizzyExternalKindFunction:
        c_import_description.desc.function_type =
            wrap(module.typesec[import.desc.function_type_index]);
        break;
    case FizzyExternalKindTable:
        c_import_description.desc.table_limits = wrap(import.desc.table.limits);
        break;
    case FizzyExternalKindMemory:
        c_import_description.desc.memory_limits = wrap(import.desc.memory.limits);
        break;
    case FizzyExternalKindGlobal:
        c_import_description.desc.global_type = wrap(import.desc.global);
        break;
    }
    return c_import_description;
}

inline FizzyExportDescription wrap(const fizzy::Export& exp) noexcept
{
    return {exp.name.c_str(), wrap(exp.kind), exp.index};
}
}  // namespace

extern "C" {

bool fizzy_validate(const uint8_t* wasm_binary, size_t wasm_binary_size, FizzyError* error) noexcept
{
    try
    {
        fizzy::parse({wasm_binary, wasm_binary_size});
        set_success(error);
        return true;
    }
    catch (...)
    {
        set_error_from_current_exception(error);
        return false;
    }
}

const FizzyModule* fizzy_parse(
    const uint8_t* wasm_binary, size_t wasm_binary_size, FizzyError* error) noexcept
{
    try
    {
        auto module = fizzy::parse({wasm_binary, wasm_binary_size});
        set_success(error);
        return wrap(module.release());
    }
    catch (...)
    {
        set_error_from_current_exception(error);
        return nullptr;
    }
}

void fizzy_free_module(const FizzyModule* module) noexcept
{
    delete unwrap(module);
}

const FizzyModule* fizzy_clone_module(const FizzyModule* module) noexcept
{
    try
    {
        auto clone = std::make_unique<fizzy::Module>(*unwrap(module));
        return wrap(clone.release());
    }
    catch (...)
    {
        return nullptr;
    }
}

uint32_t fizzy_get_type_count(const FizzyModule* module) noexcept
{
    return static_cast<uint32_t>(unwrap(module)->typesec.size());
}

FizzyFunctionType fizzy_get_type(const FizzyModule* module, uint32_t type_idx) noexcept
{
    return wrap(unwrap(module)->typesec[type_idx]);
}

uint32_t fizzy_get_import_count(const FizzyModule* module) noexcept
{
    return static_cast<uint32_t>(unwrap(module)->importsec.size());
}

FizzyImportDescription fizzy_get_import_description(
    const FizzyModule* c_module, uint32_t import_idx) noexcept
{
    const auto* module = unwrap(c_module);
    return wrap(module->importsec[import_idx], *module);
}

FizzyFunctionType fizzy_get_function_type(const FizzyModule* module, uint32_t func_idx) noexcept
{
    return wrap(unwrap(module)->get_function_type(func_idx));
}

bool fizzy_module_has_table(const FizzyModule* module) noexcept
{
    return unwrap(module)->has_table();
}

bool fizzy_module_has_memory(const FizzyModule* module) noexcept
{
    return unwrap(module)->has_memory();
}

uint32_t fizzy_get_global_count(const FizzyModule* module) noexcept
{
    return static_cast<uint32_t>(unwrap(module)->get_global_count());
}

FizzyGlobalType fizzy_get_global_type(const FizzyModule* module, uint32_t global_idx) noexcept
{
    return wrap(unwrap(module)->get_global_type(global_idx));
}

uint32_t fizzy_get_export_count(const FizzyModule* module) noexcept
{
    return static_cast<uint32_t>(unwrap(module)->exportsec.size());
}

FizzyExportDescription fizzy_get_export_description(
    const FizzyModule* module, uint32_t export_idx) noexcept
{
    return wrap(unwrap(module)->exportsec[export_idx]);
}

bool fizzy_find_exported_function_index(
    const FizzyModule* module, const char* name, uint32_t* out_func_idx) noexcept
{
    const auto optional_func_idx = fizzy::find_exported_function_index(*unwrap(module), name);
    if (!optional_func_idx)
        return false;

    *out_func_idx = *optional_func_idx;
    return true;
}

bool fizzy_find_exported_function(
    FizzyInstance* instance, const char* name, FizzyExternalFunction* out_function) noexcept
{
    auto optional_func = fizzy::find_exported_function(*unwrap(instance), name);
    if (!optional_func)
        return false;

    try
    {
        *out_function = wrap(std::move(*optional_func));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void fizzy_free_exported_function(FizzyExternalFunction* external_function) noexcept
{
    delete static_cast<fizzy::ExternalFunction*>(external_function->context);
}

bool fizzy_find_exported_table(
    FizzyInstance* instance, const char* name, FizzyExternalTable* out_table) noexcept
{
    const auto optional_external_table = fizzy::find_exported_table(*unwrap(instance), name);
    if (!optional_external_table)
        return false;

    *out_table = wrap(*optional_external_table);
    return true;
}

bool fizzy_find_exported_memory(
    FizzyInstance* instance, const char* name, FizzyExternalMemory* out_memory) noexcept
{
    const auto optional_external_memory = fizzy::find_exported_memory(*unwrap(instance), name);
    if (!optional_external_memory)
        return false;

    *out_memory = wrap(*optional_external_memory);
    return true;
}

bool fizzy_find_exported_global(
    FizzyInstance* instance, const char* name, FizzyExternalGlobal* out_global) noexcept
{
    const auto optional_external_global = fizzy::find_exported_global(*unwrap(instance), name);
    if (!optional_external_global)
        return false;

    *out_global = wrap(*optional_external_global);
    return true;
}

bool fizzy_module_has_start_function(const FizzyModule* module) noexcept
{
    return unwrap(module)->startfunc.has_value();
}

FizzyInstance* fizzy_instantiate(const FizzyModule* module,
    const FizzyExternalFunction* imported_functions, size_t imported_functions_size,
    const FizzyExternalTable* imported_table, const FizzyExternalMemory* imported_memory,
    const FizzyExternalGlobal* imported_globals, size_t imported_globals_size,
    uint32_t memory_pages_limit, FizzyError* error) noexcept
{
    try
    {
        auto functions = unwrap(imported_functions, imported_functions_size);
        auto table = unwrap(imported_table);
        auto memory = unwrap(imported_memory);
        auto globals = unwrap(imported_globals, imported_globals_size);

        auto instance = fizzy::instantiate(std::unique_ptr<const fizzy::Module>(unwrap(module)),
            std::move(functions), std::move(table), std::move(memory), std::move(globals),
            memory_pages_limit);

        set_success(error);
        return wrap(instance.release());
    }
    catch (...)
    {
        set_error_from_current_exception(error);
        return nullptr;
    }
}

FizzyInstance* fizzy_resolve_instantiate(const FizzyModule* c_module,
    const FizzyImportedFunction* c_imported_functions, size_t imported_functions_size,
    const FizzyExternalTable* imported_table, const FizzyExternalMemory* imported_memory,
    const FizzyImportedGlobal* c_imported_globals, size_t imported_globals_size,
    uint32_t memory_pages_limit, FizzyError* error) noexcept
{
    try
    {
        auto imported_functions = unwrap(c_imported_functions, imported_functions_size);
        auto table = unwrap(imported_table);
        auto memory = unwrap(imported_memory);
        auto imported_globals = unwrap(c_imported_globals, imported_globals_size);

        std::unique_ptr<const fizzy::Module> module{unwrap(c_module)};
        auto resolved_imports = fizzy::resolve_imported_functions(*module, imported_functions);
        auto resolved_globals = fizzy::resolve_imported_globals(*module, imported_globals);

        auto instance = fizzy::instantiate(std::move(module), std::move(resolved_imports),
            std::move(table), std::move(memory), std::move(resolved_globals), memory_pages_limit);

        set_success(error);
        return wrap(instance.release());
    }
    catch (...)
    {
        set_error_from_current_exception(error);
        return nullptr;
    }
}

void fizzy_free_instance(FizzyInstance* instance) noexcept
{
    delete unwrap(instance);
}

const FizzyModule* fizzy_get_instance_module(FizzyInstance* instance) noexcept
{
    return wrap(unwrap(instance)->module.get());
}

uint8_t* fizzy_get_instance_memory_data(FizzyInstance* instance) noexcept
{
    auto& memory = unwrap(instance)->memory;
    if (!memory)
        return nullptr;

    return memory->data();
}

size_t fizzy_get_instance_memory_size(FizzyInstance* instance) noexcept
{
    auto& memory = unwrap(instance)->memory;
    if (!memory)
        return 0;

    return memory->size();
}

FizzyExecutionContext* fizzy_create_execution_context(int depth) noexcept
{
    auto ctx = std::make_unique<fizzy::ExecutionContext>();
    ctx->depth = depth;
    return wrap(ctx.release());
}

FizzyExecutionContext* fizzy_create_metered_execution_context(int depth, int64_t ticks) noexcept
{
    auto ctx = std::make_unique<fizzy::ExecutionContext>();
    ctx->depth = depth;
    ctx->metering_enabled = true;
    ctx->ticks = ticks;
    return wrap(ctx.release());
}

void fizzy_free_execution_context(FizzyExecutionContext* c_ctx) noexcept
{
    delete unwrap(c_ctx);
}

int* fizzy_get_execution_context_depth(FizzyExecutionContext* c_ctx) noexcept
{
    return &unwrap(c_ctx)->depth;
}

int64_t* fizzy_get_execution_context_ticks(FizzyExecutionContext* c_ctx) noexcept
{
    return &unwrap(c_ctx)->ticks;
}

FizzyExecutionResult fizzy_execute(FizzyInstance* c_instance, uint32_t func_idx,
    const FizzyValue* c_args, FizzyExecutionContext* c_ctx) noexcept
{
    auto* instance = unwrap(c_instance);
    const auto* args = unwrap(c_args);
    const auto result =
        (c_ctx == nullptr ? fizzy::execute(*instance, func_idx, args) :
                            fizzy::execute(*instance, func_idx, args, *unwrap(c_ctx)));
    return wrap(result);
}

}  // extern "C"
