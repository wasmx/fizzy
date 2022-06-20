// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

/// Fizzy public C API.
/// @file
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// Safe way of marking a function with `noexcept` C++ specifier.
#ifdef __cplusplus
#define FIZZY_NOEXCEPT noexcept
#else
#define FIZZY_NOEXCEPT
#endif

#ifdef __cplusplus
extern "C" {
#endif

/// Error codes.
typedef enum FizzyErrorCode
{
    /// Success.
    FizzySuccess = 0,
    /// Malformed module.
    FizzyErrorMalformedModule,
    /// Invalid module.
    FizzyErrorInvalidModule,
    /// Instantiation failed.
    FizzyErrorInstantiationFailed,
    /// Memory allocation failed.
    FizzyErrorMemoryAllocationFailed,
    /// Other error.
    FizzyErrorOther
} FizzyErrorCode;

enum
{
    /// Default hard limit of the memory size (256MB) to call fizzy_instantiate() and
    /// fizzy_resolve_instantiate().
    FizzyMemoryPagesLimitDefault = 4096
};

/// Error information.
typedef struct FizzyError
{
    /// Error code.
    FizzyErrorCode code;
    /// NULL-terminated error message.
    char message[256];
} FizzyError;

/// The opaque data type representing a module.
typedef struct FizzyModule FizzyModule;

/// The opaque data type representing an instance (instantiated module).
typedef struct FizzyInstance FizzyInstance;

/// The data type representing numeric values.
typedef union FizzyValue
{
    /// 32-bit integer value.
    uint32_t i32;
    /// 64-bit integer value.
    uint64_t i64;
    /// 32-bit floating-point value.
    float f32;
    /// 64-bit floating-point value.
    double f64;
} FizzyValue;

/// Result of execution of a function.
typedef struct FizzyExecutionResult
{
    /// Whether execution ended with a trap.
    bool trapped;
    /// Whether function returned a value.
    /// Equals false if #trapped equals true.
    bool has_value;
    /// Value returned from a function.
    /// Valid only if #has_value equals true.
    FizzyValue value;
} FizzyExecutionResult;

/// The opaque data type representing an execution context.
typedef struct FizzyExecutionContext FizzyExecutionContext;


/// Pointer to external function.
///
/// @param  host_ctx    Opaque pointer to host context.
/// @param  instance    Pointer to module instance.
/// @param  args        Pointer to the argument array. Can be NULL iff function has no inputs.
/// @param  ctx         Opaque pointer to execution context. If NULL new execution context
///                     will be allocated.
/// @return             Result of execution.
///
/// @note
/// External functions implemented in C++ must be non-throwing, i.e. the effect of any exception
/// escaping the function is std::terminate being called.
typedef FizzyExecutionResult (*FizzyExternalFn)(void* host_ctx, FizzyInstance* instance,
    const FizzyValue* args, FizzyExecutionContext* ctx) FIZZY_NOEXCEPT;

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
    /// Output type, equals to ::FizzyValueTypeVoid iff function has no output.
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

/// Global type.
typedef struct FizzyGlobalType
{
    FizzyValueType value_type;
    bool is_mutable;
} FizzyGlobalType;

/// The opaque data type representing a table.
typedef struct FizzyTable FizzyTable;

/// Limits.
typedef struct FizzyLimits
{
    /// Minimum value.
    uint32_t min;
    /// Maximum value.
    /// Valid only if #has_max equals true.
    uint32_t max;
    /// Whether limits has maximum value.
    bool has_max;
} FizzyLimits;

/// External table.
typedef struct FizzyExternalTable
{
    /// Opaque pointer to table data.
    FizzyTable* table;
    /// Table limits.
    FizzyLimits limits;
} FizzyExternalTable;

/// The opaque data type representing a memory.
typedef struct FizzyMemory FizzyMemory;

/// External memory.
typedef struct FizzyExternalMemory
{
    /// Opaque pointer to memory data.
    FizzyMemory* memory;
    /// Memory limits.
    FizzyLimits limits;
} FizzyExternalMemory;

/// External global.
typedef struct FizzyExternalGlobal
{
    /// Pointer to global value. Cannot be NULL.
    FizzyValue* value;
    /// Type of global.
    FizzyGlobalType type;
} FizzyExternalGlobal;

/// External kind.
typedef enum FizzyExternalKind
{
    FizzyExternalKindFunction,
    FizzyExternalKindTable,
    FizzyExternalKindMemory,
    FizzyExternalKindGlobal
} FizzyExternalKind;

/// Import description.
///
/// @note Only one member of #desc union corresponding to #kind is defined for each import.
/// @note For the lifetime of the #module and #name fields, please refer to the description provided
/// by the function used to obtain this structure.
typedef struct FizzyImportDescription
{
    /// Import's module name.
    const char* module;
    /// Import name.
    const char* name;
    /// Import kind.
    FizzyExternalKind kind;
    /// Import type definition.
    union
    {
        FizzyFunctionType function_type;
        FizzyLimits memory_limits;
        FizzyLimits table_limits;
        FizzyGlobalType global_type;
    } desc;
} FizzyImportDescription;

/// Export description.
///
/// @note For the lifetime of the #name field, please refer to the description provided by the
/// function used to obtain this structure.
typedef struct FizzyExportDescription
{
    /// Export name.
    const char* name;
    /// Export kind.
    FizzyExternalKind kind;
    /// Index of exported function or table or memory or global.
    /// #kind determines what is this the index of.
    uint32_t index;
} FizzyExportDescription;

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

/// Imported global.
typedef struct FizzyImportedGlobal
{
    /// Module name. NULL-terminated string. Cannot be NULL.
    const char* module;
    /// Function name. NULL-terminated string. Cannot be NULL.
    const char* name;
    /// External global, defining its value type, and a pointer to value.
    FizzyExternalGlobal external_global;
} FizzyImportedGlobal;

/// Validate binary module.
///
/// @param  wasm_binary         Pointer to module binary data.
/// @param  wasm_binary_size    Size of the module binary data.
/// @param  error               Pointer to store detailed error information at. Can be NULL if error
///                             information is not required.
/// @return                     true if module is valid, false otherwise.
///
/// @note   FizzyError::code will be ::FizzySuccess if function returns `true` and will not be
///         ::FizzySuccess otherwise.
bool fizzy_validate(
    const uint8_t* wasm_binary, size_t wasm_binary_size, FizzyError* error) FIZZY_NOEXCEPT;

/// Parse binary module.
///
/// @param  wasm_binary         Pointer to module binary data.
/// @param  wasm_binary_size    Size of the module binary data.
/// @param  error               Pointer to store detailed error information at. Can be NULL if error
///                             information is not required.
/// @return                     non-NULL pointer to module in case of success, NULL otherwise.
///
/// @note   FizzyError::code will be ::FizzySuccess if function returns non-NULL
///         will and will not be ::FizzySuccess otherwise.
const FizzyModule* fizzy_parse(
    const uint8_t* wasm_binary, size_t wasm_binary_size, FizzyError* error) FIZZY_NOEXCEPT;

/// Free resources associated with the module.
///
/// @param  module    Pointer to module. If NULL is passed, function has no effect.
///
/// @note
/// Should be called unless @p module was passed to fizzy_instantiate().
void fizzy_free_module(const FizzyModule* module) FIZZY_NOEXCEPT;

/// Make a copy of a module.
///
/// @param  module    Pointer to module. Cannot be NULL.
/// @return           Pointer to a newly allocated module, identical to @p module, or NULL in case
///                   memory for a module could not be allocated.
///
/// @note  Creating a copy is needed if more than single instance of a module is required, because
/// instantiation takes ownership of a module, and the same module cannot be instantiated twice.
/// @note  Input module is not modified neither in success nor in failure case.
const FizzyModule* fizzy_clone_module(const FizzyModule* module) FIZZY_NOEXCEPT;

/// Get number of types defined in the module.
///
/// @param  module    Pointer to module. Cannot be NULL.
/// @return           Number of type in the module.
uint32_t fizzy_get_type_count(const FizzyModule* module) FIZZY_NOEXCEPT;

/// Get type defined in the module.
///
/// @param  module      Pointer to module. Cannot be NULL.
/// @param  type_idx    Type index. Behaviour is undefined if index is not valid according
///                     to module definition.
/// @return             Type corresponding to the index.
FizzyFunctionType fizzy_get_type(const FizzyModule* module, uint32_t type_idx) FIZZY_NOEXCEPT;

/// Get number of imports defined in the module.
///
/// @param  module    Pointer to module. Cannot be NULL.
/// @return           Number of imports in the module.
uint32_t fizzy_get_import_count(const FizzyModule* module) FIZZY_NOEXCEPT;

/// Get the import description defined in the module.
///
/// @param  module        Pointer to module. Cannot be NULL.
/// @param  import_idx    Import index. Behaviour is undefined if index is not valid according
///                       to module definition.
/// @return               Type of the import corresponding to the index.
///                       FizzyImportDescription::module and FizzyImportDescription::name fields
///                       point to the string stored inside the module and are valid as long as
///                       module is alive (including after successful instantiation.)
FizzyImportDescription fizzy_get_import_description(
    const FizzyModule* module, uint32_t import_idx) FIZZY_NOEXCEPT;

/// Get type of the function defined in the module.
///
/// @param  module      Pointer to module. Cannot be NULL.
/// @param  func_idx    Function index. Can be either index of an imported function or of a function
///                     defined in module. Behaviour is undefined if index is not valid according
///                     to module definition.
/// @return             Type of the function corresponding to the index.
///
/// @note  All module function indices are greater than all imported function indices.
FizzyFunctionType fizzy_get_function_type(
    const FizzyModule* module, uint32_t func_idx) FIZZY_NOEXCEPT;

/// Check whether module has a table.
///
/// @param  module          Pointer to module. Cannot be NULL.
/// @return                 true if module has a table definition, false otherwise.
bool fizzy_module_has_table(const FizzyModule* module) FIZZY_NOEXCEPT;

/// Check whether module has a memory.
///
/// @param  module          Pointer to module. Cannot be NULL.
/// @return                 true if module has a memory definition, false otherwise.
bool fizzy_module_has_memory(const FizzyModule* module) FIZZY_NOEXCEPT;

/// Get number of globals defined in the module.
///
/// @param  module    Pointer to module. Cannot be NULL.
/// @return           Number of globals in the module.
uint32_t fizzy_get_global_count(const FizzyModule* module) FIZZY_NOEXCEPT;

/// Get type of a given global defined in the module.
///
/// @param  module        Pointer to module. Cannot be NULL.
/// @param  global_idx    Global index. Can be either index of an imported global or of a global
///                       defined in module. Behaviour is undefined if index is not valid according
///                       to module definition.
/// @return               Type of the global corresponding to the index.
///
/// @note  All module global indices are greater than all imported global indices.
FizzyGlobalType fizzy_get_global_type(
    const FizzyModule* module, uint32_t global_idx) FIZZY_NOEXCEPT;

/// Get number of exports defined in the module.
///
/// @param  module    Pointer to module. Cannot be NULL.
/// @return           Number of exports in the module.
uint32_t fizzy_get_export_count(const FizzyModule* module) FIZZY_NOEXCEPT;

/// Get the export description defined in the module.
///
/// @param  module        Pointer to module. Cannot be NULL.
/// @param  export_idx    Export index. Behaviour is undefined if index is not valid according
///                       to module definition.
/// @return               Description of the export corresponding to the index.
///                       FizzyExportDescription::name field points to the string stored inside the
///                       module and is valid as long as module is alive (including after successful
///                       instantiation.)
FizzyExportDescription fizzy_get_export_description(
    const FizzyModule* module, uint32_t export_idx) FIZZY_NOEXCEPT;

/// Find index of exported function by name.
///
/// @param  module          Pointer to module. Cannot be NULL.
/// @param  name            The function name. NULL-terminated string. Cannot be NULL.
/// @param  out_func_idx    Pointer to output where function index will be stored. Cannot be NULL.
/// @return                 true if function was found, false otherwise.
bool fizzy_find_exported_function_index(
    const FizzyModule* module, const char* name, uint32_t* out_func_idx) FIZZY_NOEXCEPT;

/// Check whether module has a start function.
///
/// @param  module          Pointer to module. Cannot be NULL.
/// @return                 true if module has a start function, false otherwise.
bool fizzy_module_has_start_function(const FizzyModule* module) FIZZY_NOEXCEPT;

/// Instantiate a module.
///
/// The instance takes ownership of the module, i.e. fizzy_free_module() must not be called on the
/// module after this call.
/// For simplicity a module cannot be shared among several instances (calling fizzy_instatiate()
/// more than once with the same module results in undefined behaviour), but after
/// fizzy_instantiate() functions querying module info can still be called with @p module.
///
/// @param  module                     Pointer to module. Cannot be NULL.
/// @param  imported_functions         Pointer to the imported function array. Can be NULL iff
///                                    @p imported_functions_size equals 0.
/// @param  imported_functions_size    Size of the imported function array. Can be zero.
/// @param  imported_table             Pointer to the imported table. Can be NULL iff module doesn't
///                                    import a table. Not an array, because WebAssembly 1.0 doesn't
///                                    support more than one table in a module.
/// @param  imported_memory            Pointer to the imported memory. Can be NULL iff module
///                                    doesn't import a memory. Not an array, because WebAssembly
///                                    1.0 doesn't support more than one memory in a module.
/// @param  imported_globals           Pointer to the imported globals array. Can be NULL iff
///                                    @p imported_globals_size equals 0.
/// @param  imported_globals_size      Size of the imported global array. Can be zero.
/// @param  memory_pages_limit         Hard limit for memory growth in pages. Cannot be above 65536.
/// @param  error                      Pointer to store detailed error information at. Can be NULL
///                                    if error information is not required.
/// @return                            non-NULL pointer to instance in case of success,
///                                    NULL otherwise.
///
/// @note
/// Function expects @p imported_functions to be in the order of imports defined in the module.
/// No validation is done on the number of functions passed in, nor on their order.
/// When number of passed functions or their order is different from the one defined by the
/// module, behaviour is undefined.
///
/// @note
/// Function expects @p imported_globals to be in the order of imports defined in the module.
/// No validation is done on the number of globals passed in, nor on their order.
/// When number of passed globals or their order is different from the one defined by the
/// module, behaviour is undefined.
///
/// @note
/// FizzyError::code will be ::FizzySuccess if function returns non-NULL pointer and will not be
/// ::FizzySuccess otherwise.
FizzyInstance* fizzy_instantiate(const FizzyModule* module,
    const FizzyExternalFunction* imported_functions, size_t imported_functions_size,
    const FizzyExternalTable* imported_table, const FizzyExternalMemory* imported_memory,
    const FizzyExternalGlobal* imported_globals, size_t imported_globals_size,
    uint32_t memory_pages_limit, FizzyError* error) FIZZY_NOEXCEPT;

/// Instantiate a module resolving imported functions.
///
/// The instance takes ownership of the module, i.e. fizzy_free_module() must not be called on the
/// module after this call.
/// For simplicity a module cannot be shared among several instances (calling
/// fizzy_resolve_instatiate() more than once with the same module results in undefined behaviour),
/// but after fizzy_resolve_instantiate() functions querying module info can still be called with
/// @p module.
///
/// @param  module                     Pointer to module. Cannot be NULL.
/// @param  imported_functions         Pointer to the imported function array. Can be NULL iff
///                                    @p imported_functions_size equals 0.
/// @param  imported_functions_size    Size of the imported function array. Can be zero.
/// @param  imported_table             Pointer to the imported table. Can be NULL iff module doesn't
///                                    import a table. Not an array, because WebAssembly 1.0 doesn't
///                                    support more than one table in a module.
/// @param  imported_memory            Pointer to the imported memory. Can be NULL iff module
///                                    doesn't import a memory. Not an array, because WebAssembly
///                                    1.0 doesn't support more than one memory in a module.
/// @param  imported_globals           Pointer to the imported globals array. Can be NULL iff
///                                    @p imported_globals_size equals 0.
/// @param  imported_globals_size      Size of the imported global array. Can be zero.
/// @param  memory_pages_limit         Hard limit for memory growth in pages. Cannot be above 65536.
/// @param  error                      Pointer to store detailed error information at. Can be NULL
///                                    if error information is not required.
/// @return                            non-NULL pointer to instance in case of success,
///                                    NULL otherwise.
///
/// @note
/// Functions in @p imported_functions are allowed to be in any order and allowed to include some
/// functions not required by the module.
/// Functions are matched to module's imports based on their FizzyImportedFunction::module and
/// FizzyImportedFunction::name strings.
///
/// @note
/// Globals in @a imported_globals are allowed to be in any order and allowed to include some
/// globals not required by the module.
/// Globals are matched to module's imports based on their module and name strings.
///
/// @note
/// FizzyImportedFunction::module, FizzyImportedFunction::name, FizzyImportedGlobal::module,
/// FizzyImportedGlobal::name strings need to be valid only until fizzy_resolve_instantiate()
/// returns.
///
/// @note
/// FizzyError::code will be ::FizzySuccess if function returns non-NULL pointer and will not be
/// ::FizzySuccess otherwise.
FizzyInstance* fizzy_resolve_instantiate(const FizzyModule* module,
    const FizzyImportedFunction* imported_functions, size_t imported_functions_size,
    const FizzyExternalTable* imported_table, const FizzyExternalMemory* imported_memory,
    const FizzyImportedGlobal* imported_globals, size_t imported_globals_size,
    uint32_t memory_pages_limit, FizzyError* error) FIZZY_NOEXCEPT;

/// Free resources associated with the instance.
///
/// @param  instance    Pointer to instance. If NULL is passed, function has no effect.
void fizzy_free_instance(FizzyInstance* instance) FIZZY_NOEXCEPT;

/// Get pointer to module of an instance.
///
/// @param  instance    Pointer to instance. Cannot be NULL.
/// @return             Pointer to a module.
///
/// @note    The returned pointer represents non-owning, "view"-access to the module and must not be
///          passed to fizzy_free_module().
const FizzyModule* fizzy_get_instance_module(FizzyInstance* instance) FIZZY_NOEXCEPT;

/// Get pointer to memory of an instance.
///
/// @param  instance    Pointer to instance. Cannot be NULL.
/// @return             Pointer to memory data or NULL in case instance doesn't have any memory.
///
/// @note    Function returns pointer to memory regardless of whether memory is exported or not.
/// @note    For instances of the modules defined with memory of size 0 the returned pointer is not
///          NULL.
uint8_t* fizzy_get_instance_memory_data(FizzyInstance* instance) FIZZY_NOEXCEPT;

/// Get size of memory of an instance.
///
/// @param  instance    Pointer to instance. Cannot be NULL.
/// @return             Size of memory in bytes or 0 in case instance doesn't have any memory.
///
/// @note    Function returns memory size regardless of whether memory is exported or not.
size_t fizzy_get_instance_memory_size(FizzyInstance* instance) FIZZY_NOEXCEPT;

/// Find exported function by name.
///
/// @param  instance        Pointer to instance. Cannot be NULL.
/// @param  name            The function name. NULL-terminated string. Cannot be NULL.
/// @param  out_function    Pointer to output struct to store the found function. Cannot be NULL.
///                         If function is found, associated context is allocated, which must exist
///                         as long as the function can be called by some other instance, and should
///                         be destroyed with fizzy_free_exported_function() afterwards.
///                         When function is not found (false returned), this @p out_function is not
///                         modified, and fizzy_free_exported_function() must not be called.
/// @return                 true if function was found, false otherwise.
bool fizzy_find_exported_function(
    FizzyInstance* instance, const char* name, FizzyExternalFunction* out_function) FIZZY_NOEXCEPT;

/// Free resources associated with exported function.
///
/// @param  external_function    Pointer to external function struct filled by
///                              fizzy_find_exported_function(). Cannot be NULL.
///
/// @note  This function may not be called with external function, which was not returned from
///        fizzy_find_exported_function().
void fizzy_free_exported_function(FizzyExternalFunction* external_function) FIZZY_NOEXCEPT;

/// Find exported table by name.
///
/// @param  instance     Pointer to instance. Cannot be NULL.
/// @param  name         The table name. NULL-terminated string. Cannot be NULL.
/// @param  out_table    Pointer to output struct to store found table. Cannot be NULL.
/// @return              true if table was found, false otherwise.
///
/// @note  WebAssembly 1.0 spec allows at most one table in a module.
bool fizzy_find_exported_table(
    FizzyInstance* instance, const char* name, FizzyExternalTable* out_table) FIZZY_NOEXCEPT;

/// Find exported memory by name.
///
/// @param  instance      Pointer to instance. Cannot be NULL.
/// @param  name          The table name. NULL-terminated string. Cannot be NULL.
/// @param  out_memory    Pointer to output struct to store found memory. Cannot be NULL.
/// @return               true if memory was found, false otherwise.
///
/// @note  WebAssembly 1.0 spec allows at most one memory in a module.
bool fizzy_find_exported_memory(
    FizzyInstance* instance, const char* name, FizzyExternalMemory* out_memory) FIZZY_NOEXCEPT;

/// Find exported global by name.
///
/// @param  instance      Pointer to instance. Cannot be NULL.
/// @param  name          The global name. NULL-terminated string. Cannot be NULL.
/// @param  out_global    Pointer to output struct to store found global. Cannot be NULL.
/// @return               true if global was found, false otherwise.
bool fizzy_find_exported_global(
    FizzyInstance* instance, const char* name, FizzyExternalGlobal* out_global) FIZZY_NOEXCEPT;

FizzyExecutionContext* fizzy_create_execution_context(int depth) FIZZY_NOEXCEPT;

FizzyExecutionContext* fizzy_create_metered_execution_context(
    int depth, int64_t ticks) FIZZY_NOEXCEPT;

void fizzy_free_execution_context(FizzyExecutionContext* ctx) FIZZY_NOEXCEPT;

int* fizzy_get_execution_context_depth(FizzyExecutionContext* ctx) FIZZY_NOEXCEPT;

int64_t* fizzy_get_execution_context_ticks(FizzyExecutionContext* ctx) FIZZY_NOEXCEPT;

/// Execute module function.
///
/// @param  instance    Pointer to module instance. Cannot be NULL.
/// @param  args        Pointer to the argument array. Can be NULL if function has 0 inputs.
/// @param  ctx         Opaque pointer to execution context. If NULL new execution context
///                     will be allocated.
/// @return             Result of execution.
///
/// @note
/// No validation is done on the number of arguments passed in @p args, nor on their types.
/// When number of passed arguments or their types are different from the ones defined by the
/// function type, behaviour is undefined.
FizzyExecutionResult fizzy_execute(FizzyInstance* instance, uint32_t func_idx,
    const FizzyValue* args, FizzyExecutionContext* ctx) FIZZY_NOEXCEPT;

#ifdef __cplusplus
}
#endif
