// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>

bool validate(const uint8_t* binary, size_t size);
bool parse(const uint8_t* binary, size_t size);
bool instantiate(const uint8_t* binary, size_t size);
struct FizzyExecutionResult dummy_host_func(void* context, struct FizzyInstance* instance,
    const union FizzyValue* args, size_t args_size, int depth);
bool instantiate_with_host_func(const uint8_t* binary, size_t size);
bool execute(const uint8_t* binary, size_t size);

bool validate(const uint8_t* binary, size_t size)
{
    return fizzy_validate(binary, size);
}

bool parse(const uint8_t* binary, size_t size)
{
    struct FizzyModule* module = fizzy_parse(binary, size);
    if (!module)
        return false;

    fizzy_free_module(module);
    return true;
}

bool instantiate(const uint8_t* binary, size_t size)
{
    struct FizzyModule* module = fizzy_parse(binary, size);
    if (!module)
        return false;

    struct FizzyInstance* instance = fizzy_instantiate(module, NULL, 0);
    if (!instance)
        return false;

    fizzy_free_instance(instance);
    return true;
}

struct FizzyExecutionResult dummy_host_func(void* context, struct FizzyInstance* instance,
    const union FizzyValue* args, size_t args_size, int depth)
{
    (void)context;
    (void)instance;
    (void)args;
    (void)args_size;
    (void)depth;
    struct FizzyExecutionResult res = {true, false, {0}};
    return res;
}

bool instantiate_with_host_func(const uint8_t* binary, size_t size)
{
    struct FizzyModule* module = fizzy_parse(binary, size);
    if (!module)
        return false;

    FizzyExternalFunction host_funcs[] = {{dummy_host_func, NULL}};

    struct FizzyInstance* instance = fizzy_instantiate(module, host_funcs, 1);
    if (!instance)
        return false;

    fizzy_free_instance(instance);
    return true;
}

bool execute(const uint8_t* binary, size_t size)
{
    struct FizzyModule* module = fizzy_parse(binary, size);
    if (!module)
        return false;

    struct FizzyInstance* instance = fizzy_instantiate(module, NULL, 0);
    if (!instance)
        return false;

    fizzy_execute(instance, 0, NULL, 0);

    union FizzyValue args[] = {{1}, {2}};
    fizzy_execute(instance, 1, args, 0);

    fizzy_free_instance(instance);
    return true;
}
