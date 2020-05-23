// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "wasm_export.h"

#include <test/utils/hex.hpp>
#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>
#include <iostream>
#include <ostream>
#include <sstream>

namespace fizzy::test
{
static_assert(sizeof(wasm_function_inst_t) <= sizeof(WasmEngine::FuncRef));

class WAMREngine : public WasmEngine
{
    wasm_module_inst_t m_instance{nullptr};
    wasm_exec_env_t m_env{nullptr};

public:
    WAMREngine()
    {
        if (!wasm_runtime_init())
            abort();
    }
    ~WAMREngine()
    {
        if (m_env != nullptr)
            wasm_runtime_destroy_exec_env(m_env);
        if (m_instance != nullptr)
            wasm_runtime_deinstantiate(m_instance);
        wasm_runtime_destroy();
    }

    bool parse(bytes_view input) const final;
    std::optional<FuncRef> find_function(std::string_view name) const final;
    bool instantiate(bytes_view wasm_binary) final;
    bool init_memory(fizzy::bytes_view memory) final;
    fizzy::bytes_view get_memory() const final;
    Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) final;
};

std::unique_ptr<WasmEngine> create_wamr_engine()
{
    return std::make_unique<WAMREngine>();
}

bool WAMREngine::parse(bytes_view _input) const
{
    char errors[256] = {0};
    // NOTE: making an explicit copy here because in some cases wamr modifies the input...
    bytes input{_input};
    //    std::cout << static_cast<int>(input[37]) << std::endl;
    auto module = wasm_runtime_load(
        input.data(), static_cast<uint32_t>(input.size()), errors, sizeof(errors));
    if (module == nullptr)
    {
        std::cout << errors << std::endl;
        return false;
    }
    wasm_runtime_unload(module);
    return true;
}

bool WAMREngine::instantiate(bytes_view _wasm_binary)
{
    char errors[256] = {0};
    // NOTE: making an explicit copy here because in some cases wamr modifies the input...
    bytes wasm_binary{_wasm_binary};
    //    std::cout << static_cast<int>(wasm_binary[37]) << std::endl;
    //    std::cout << hex(wasm_binary) << std::endl;
    auto module = wasm_runtime_load(
        wasm_binary.data(), static_cast<uint32_t>(wasm_binary.size()), errors, sizeof(errors));
    if (module == nullptr)
    {
        std::cout << errors << std::endl;
        return false;
    }
    // If these are set to 0, the defaults are used.
    uint32_t stack_size = 8092;
    uint32_t heap_size = 8092;
    m_instance = wasm_runtime_instantiate(module, stack_size, heap_size, errors, sizeof(errors));
    if (m_instance == nullptr)
    {
        std::cout << errors << std::endl;
        wasm_runtime_unload(module);
        return false;
    }
    m_env = wasm_runtime_create_exec_env(m_instance, 8092);
    if (m_env == nullptr)
    {
        wasm_runtime_deinstantiate(m_instance);
        return false;
    }
    return true;
}

bool WAMREngine::init_memory(fizzy::bytes_view memory)
{
    (void)memory;
    //    uint32_t size;
    //    const auto data = m3_GetMemory(m_runtime, &size, 0);
    //    if (data == nullptr || size < memory.size())
    //        return false;
    //    std::memcpy(data, memory.data(), memory.size());
    //    return true;
    return false;
}

fizzy::bytes_view WAMREngine::get_memory() const
{
    //    uint32_t size;
    //    auto data = m3_GetMemory(m_runtime, &size, 0);
    //    if (data == nullptr)
    //        return {};
    //    return {data, size};
    return {};
}

std::optional<WasmEngine::FuncRef> WAMREngine::find_function(std::string_view name) const
{
    // Last parameter is function signature -- ignored according to documentation.
    wasm_function_inst_t function = wasm_runtime_lookup_function(m_instance, name.data(), nullptr);
    if (function != nullptr)
        return reinterpret_cast<WasmEngine::FuncRef>(function);
    return std::nullopt;
}

WasmEngine::Result WAMREngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    wasm_function_inst_t function = reinterpret_cast<wasm_function_inst_t>(func_ref);

    // FIXME: setup args
    (void)args;
    //    (func $test (export "test") (param $a i32) (param $b i32) (param $c i32) (result i32)
    std::vector<uint32_t> argv{static_cast<uint32_t>(args[0]), static_cast<uint32_t>(args[1]),
        static_cast<uint32_t>(args[1] >> 32), static_cast<uint32_t>(args[2])};
    if (wasm_runtime_call_wasm(m_env, function, 4, argv.data()) == true)
    {
        // FIXME copy results
        return {false, std::optional<uint64_t>{argv[0] || (uint64_t(argv[1]) << 32)}};
    }

    std::cout << wasm_runtime_get_exception(wasm_runtime_get_module_inst(m_env)) << std::endl;

    return {true, std::nullopt};
}
}  // namespace fizzy::test