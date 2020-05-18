// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "wasm_export.h"

#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>

namespace fizzy::test
{
static_assert(sizeof(wasm_function_inst_t) <= sizeof(WasmEngine::FuncRef));

class WAMREngine : public WasmEngine
{
    wasm_module_inst_t m_instance{nullptr};

public:
    WAMREngine()
    {
        if (!wasm_runtime_init())
            abort();
    }
    ~WAMREngine()
    {
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

bool WAMREngine::parse(bytes_view input) const
{
    char errors[256];
    auto module = wasm_runtime_load(
        input.data(), static_cast<uint32_t>(input.size()), errors, sizeof(errors));
    if (module == nullptr)
        return false;
    wasm_runtime_unload(module);
    return true;
}

bool WAMREngine::instantiate(bytes_view wasm_binary)
{
    char errors[256];
    auto module = wasm_runtime_load(
        wasm_binary.data(), static_cast<uint32_t>(wasm_binary.size()), errors, sizeof(errors));
    if (module == nullptr)
        return false;
    m_instance = wasm_runtime_instantiate(module, 0, 0, errors, sizeof(errors));
    if (m_instance == nullptr)
    {
        wasm_runtime_unload(module);
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
    (void)func_ref;
    (void)args;
    //    unsigned ret_valid;
    //    uint64_t ret_value;
    //    IM3Function function = reinterpret_cast<IM3Function>(func_ref);
    //    auto const result = m3_CallProper(
    //        function, static_cast<uint32_t>(args.size()), args.data(), &ret_valid, &ret_value);
    //    if (result == m3Err_none)
    //        return {false, ret_valid ? ret_value : std::optional<uint64_t>{}};
    return {true, std::nullopt};
}
}  // namespace fizzy::test
