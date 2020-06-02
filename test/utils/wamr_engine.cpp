// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "wasm_export.h"

#include <test/utils/hex.hpp>
#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>
#include <iostream>
#include <map>
#include <ostream>
#include <sstream>

namespace fizzy::test
{
static_assert(sizeof(wasm_function_inst_t) <= sizeof(WasmEngine::FuncRef));

class WAMREngine : public WasmEngine
{
    wasm_module_t m_module{nullptr};
    wasm_module_inst_t m_instance{nullptr};
    wasm_exec_env_t m_env{nullptr};
    // TODO: preprocess the signature into a struct
    std::map<wasm_function_inst_t, std::string> m_signatures;

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
        if (m_module != nullptr)
            wasm_runtime_unload(m_module);
        wasm_runtime_destroy();
    }

    bool parse(bytes_view input) const final;
    std::optional<FuncRef> find_function(
        std::string_view name, std::string_view signature) const final;
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
    m_module = wasm_runtime_load(
        wasm_binary.data(), static_cast<uint32_t>(wasm_binary.size()), errors, sizeof(errors));
    if (m_module == nullptr)
    {
        std::cout << errors << std::endl;
        return false;
    }
    // If these are set to 0, the defaults are used.
    uint32_t stack_size = 8192;
    uint32_t heap_size = 8192;
    m_instance = wasm_runtime_instantiate(m_module, stack_size, heap_size, errors, sizeof(errors));
    if (m_instance == nullptr)
    {
        std::cout << errors << std::endl;
        wasm_runtime_unload(m_module);
        m_module = nullptr;
        return false;
    }
    m_env = wasm_runtime_create_exec_env(m_instance, stack_size);
    if (m_env == nullptr)
    {
        wasm_runtime_unload(m_module);
        m_module = nullptr;
        wasm_runtime_deinstantiate(m_instance);
        m_instance = nullptr;
        return false;
    }
    return true;
}

bool WAMREngine::init_memory(fizzy::bytes_view memory)
{
    // NOTE: this will crash if there's no memory exported and no way to tell...

    const auto size = static_cast<uint32_t>(memory.size());

    if (!wasm_runtime_validate_app_addr(m_instance, 0, size))
        return false;

    void* ptr = wasm_runtime_addr_app_to_native(m_instance, 0);
    assert(ptr != nullptr);
    std::memcpy(reinterpret_cast<uint8_t*>(ptr), memory.data(), size);
    return true;
}

fizzy::bytes_view WAMREngine::get_memory() const
{
    // NOTE: this will crash if there's no memory exported and no way to tell...

    int32_t start;
    int32_t end;
    if (!wasm_runtime_get_app_addr_range(m_instance, 0, &start, &end))
        return {};
    const auto size = static_cast<uint32_t>(end - start);

    void* ptr = wasm_runtime_addr_app_to_native(m_instance, 0);
    assert(ptr != nullptr);
    return {reinterpret_cast<uint8_t*>(ptr), size};
    return {};
}

std::optional<WasmEngine::FuncRef> WAMREngine::find_function(
    std::string_view name, std::string_view signature) const
{
    // Last parameter is function signature -- ignored according to documentation.
    wasm_function_inst_t function = wasm_runtime_lookup_function(m_instance, name.data(), nullptr);
    if (function != nullptr)
    {
        (void)signature;
        // m_signatures[function] = signature;
        return reinterpret_cast<WasmEngine::FuncRef>(function);
    }
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
