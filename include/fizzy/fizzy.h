// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/// The opaque data type representing a module.
typedef struct FizzyModule FizzyModule;

/// The opaque data type representing an instance (instantiated module).
typedef struct FizzyInstance FizzyInstance;

/// The data type representing numeric values.
///
/// i64 member is used to represent values of both i32 and i64 type.
union FizzyValue
{
    uint64_t i64;
    float f32;
    double f64;
};

/// Result of execution of a function.
typedef struct FizzyExecutionResult
{
    /// Whether execution ended with a trap.
    bool trapped;
    /// Whether function returned a value. Valid only if trapped equals false.
    bool has_value;
    /// Value returned from a function.
    /// Valid only if trapped equals false and has_value equals true.
    union FizzyValue value;
} FizzyExecutionResult;


/// Pointer to external function.
///
/// @param context      Opaque pointer to execution context.
/// @param instance     Pointer to module instance.
/// @param args         Pointer to the argument array. Can be NULL iff function has no inputs.
/// @param depth        Call stack depth.
typedef FizzyExecutionResult (*FizzyExternalFn)(
    void* context, FizzyInstance* instance, const union FizzyValue* args, int depth);

/// Value type.
typedef uint8_t FizzyValueType;
static const FizzyValueType FizzyValueTypeI32 = 0x7f;
static const FizzyValueType FizzyValueTypeI64 = 0x7e;
static const FizzyValueType FizzyValueTypeF32 = 0x7d;
static const FizzyValueType FizzyValueTypeF64 = 0x7c;
/// Special value, can be used only as function output type.
static const FizzyValueType FizzyValueTypeVoid = 0;

/// Function type.
typedef struct FizzyFunctionType
{
    /// Output type, equals to FizzyValueTypeVoid, iff function has no output.
    FizzyValueType output;
    /// Pointer to input types array.
    const FizzyValueType* inputs;
    /// Input types array size.
    size_t inputs_size;
} FizzyFunctionType;

/// External function.
typedef struct FizzyExternalFunction
{
    /// Function type.
    FizzyFunctionType type;
    /// Pointer to function.
    FizzyExternalFn function;
    /// Opaque pointer to execution context, that will be passed to function.
    void* context;
} FizzyExternalFunction;

/// Imported function.
typedef struct FizzyImportedFunction
{
    /// Module name. NULL-terminated string. Cannot be NULL.
    const char* module;
    /// Function name. NULL-terminated string. Cannot be NULL.
    const char* name;
    /// External function, defining its type, pointer to function and context for calling it.
    FizzyExternalFunction external_function;
} FizzyImportedFunction;

/// Validate binary module.
bool fizzy_validate(const uint8_t* wasm_binary, size_t wasm_binary_size);

/// Parse binary module.
///
/// @returns non-NULL pointer to module in case of success, NULL otherwise.
const FizzyModule* fizzy_parse(const uint8_t* wasm_binary, size_t wasm_binary_size);

/// Free resources associated with the module.
///
/// Should be called unless @p module was passed to fizzy_instantiate.
/// If passed pointer is NULL, has no effect.
void fizzy_free_module(const FizzyModule* module);

/// Get type of the function defined in the module.
///
/// @param module   Pointer to module.
/// @param func_idx Function index. Can be either index of an imported function or of a function
///                 defined in module. Behaviour is undefined, if index is not valid according to
///                 module definition.
///
/// @note All module function indices are greater than all imported function indices.
FizzyFunctionType fizzy_get_function_type(const FizzyModule* module, uint32_t func_idx);

/// Find index of exported function by name.
///
/// @param  module          Pointer to module.
/// @param  name            The function name. NULL-terminated string. Cannot be NULL.
/// @param  out_func_idx    Pointer to output where function index will be stored. Cannot be NULL.
/// @returns                true if function was found, false otherwise.
bool fizzy_find_exported_function(
    const FizzyModule* module, const char* name, uint32_t* out_func_idx);

/// Instantiate a module.
/// Takes ownership of module, i.e. @p module is invalidated after this call.
///
/// @param      module                   Pointer to module.
/// @param      imported_functions       Pointer to the imported function array. Can be NULL iff
///                                      imported_functions_size equals 0.
/// @param      imported_functions_size  Size of the imported function array. Can be zero.
/// @returns    non-NULL pointer to instance in case of success, NULL otherwise.
///
/// @note
/// Function expects @a imported_functions to be in the order of imports defined in the module.
/// No validation is done on the number of functions passed in, nor on their order.
/// When number of passed functions or their order is different from the one defined by the
/// module, behaviour is undefined.
FizzyInstance* fizzy_instantiate(const FizzyModule* module,
    const FizzyExternalFunction* imported_functions, size_t imported_functions_size);

/// Instantiate a module resolving imported functions.
/// Takes ownership of module, i.e. @p module is invalidated after this call.
///
/// @param      module                   Pointer to module.
/// @param      imported_functions       Pointer to the imported function array. Can be NULL iff
///                                      imported_functions_size equals 0.
/// @param      imported_functions_size  Size of the imported function array. Can be zero.
/// @returns    non-NULL pointer to instance in case of success, NULL otherwise.
///
/// @note
/// Functions in @a imported_functions are allowed to be in any order and allowed to include some
/// functions not required by instantiated module.
/// Functions are matched to module's imports based on their module and name strings.
FizzyInstance* fizzy_resolve_instantiate(const FizzyModule* module,
    const FizzyImportedFunction* imported_functions, size_t imported_functions_size);

/// Free resources associated with the instance.
/// If passed pointer is NULL, has no effect.
void fizzy_free_instance(FizzyInstance* instance);

/// Get pointer to module of an instance.
///
/// @note The returned pointer represents non-owning, "view"-access to the module and must not be
/// passed to fizzy_free_module.
const FizzyModule* fizzy_get_instance_module(FizzyInstance* instance);

/// Get pointer to memory of an instance.
///
/// @returns Pointer to memory data or NULL in case instance doesn't have any memory.
/// @note    Function returns pointer to memory regardless of whether memory is exported or not.
uint8_t* fizzy_get_instance_memory_data(FizzyInstance* instance);

/// Get size of memory of an instance.
///
/// @returns Size of memory in bytes or 0 in case instance doesn't have any memory.
/// @note    Function returns memory size regardless of whether memory is exported or not.
size_t fizzy_get_instance_memory_size(FizzyInstance* instance);

/// Execute module function.
///
/// @param instance     Pointer to module instance.
/// @param args         Pointer to the argument array. Can be NULL if function has 0 inputs.
/// @param depth        Call stack depth.
///
/// @note
/// No validation is done on the number of arguments passed in @p args, nor on their types.
/// When number of passed arguments or their types are different from the ones defined by the
/// function type, behaviour is undefined.
FizzyExecutionResult fizzy_execute(
    FizzyInstance* instance, uint32_t func_idx, const union FizzyValue* args, int depth);

#ifdef __cplusplus
}
#endif
