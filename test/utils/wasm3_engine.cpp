// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "source/wasm3.h"

#include <test/utils/adler32.hpp>
#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>
#include <stdexcept>

namespace fizzy::test
{
static_assert(sizeof(IM3Function) <= sizeof(WasmEngine::FuncRef));

class Wasm3Engine final : public WasmEngine
{
    IM3Environment m_env{nullptr};
    IM3Runtime m_runtime{nullptr};

public:
    Wasm3Engine() : m_env{m3_NewEnvironment()} {}
    ~Wasm3Engine() final
    {
        if (m_runtime)
            m3_FreeRuntime(m_runtime);
        m3_FreeEnvironment(m_env);
    }

    bool parse(bytes_view input) const final;
    std::optional<FuncRef> find_function(
        std::string_view name, std::string_view signature) const final;
    bool instantiate(bytes_view wasm_binary) final;
    bool init_memory(fizzy::bytes_view memory) final;
    fizzy::bytes_view get_memory() const final;
    Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) final;
};

namespace
{
const void* env_adler32(
    IM3Runtime /*runtime*/, IM3ImportContext /*context*/, uint64_t* stack, void* mem) noexcept
{
    const uint32_t offset = static_cast<uint32_t>(stack[1]);
    const uint32_t length = static_cast<uint32_t>(stack[2]);
    stack[0] = fizzy::test::adler32({reinterpret_cast<uint8_t*>(mem) + offset, length});
    return m3Err_none;
}
}  // namespace

std::unique_ptr<WasmEngine> create_wasm3_engine()
{
    return std::make_unique<Wasm3Engine>();
}

bool Wasm3Engine::parse(bytes_view input) const
{
    auto env = m3_NewEnvironment();

    IM3Module module{nullptr};
    const auto err = m3_ParseModule(env, &module, input.data(), uint32_t(input.size()));
    m3_FreeModule(module);
    m3_FreeEnvironment(env);
    return err == m3Err_none;
}

bool Wasm3Engine::instantiate(bytes_view wasm_binary)
{
    // Replace runtime (e.g. instance + module)
    if (m_runtime != nullptr)
        m3_FreeRuntime(m_runtime);

    // The 64k stack size comes from wasm3/platforms/app
    m_runtime = m3_NewRuntime(m_env, 64 * 1024, nullptr);
    if (m_runtime == nullptr)
        return false;

    IM3Module module{nullptr};
    if (m3_ParseModule(m_env, &module, wasm_binary.data(), uint32_t(wasm_binary.size())) !=
        m3Err_none)
        return false;
    assert(module != nullptr);

    // Transfers ownership to runtime.
    if (m3_LoadModule(m_runtime, module) != m3Err_none)
    {
        m3_FreeModule(module);
        return false;
    }

    const auto ret = m3_LinkRawFunction(module, "env", "adler32", "i(ii)", env_adler32);
    if (ret != m3Err_none && ret != m3Err_functionLookupFailed)
        return false;

    return m3_RunStart(module) == m3Err_none;
}

bool Wasm3Engine::init_memory(fizzy::bytes_view memory)
{
    uint32_t size;
    const auto data = m3_GetMemory(m_runtime, &size, 0);
    if (data == nullptr || size < memory.size())
        return false;
    std::memcpy(data, memory.data(), memory.size());
    return true;
}

fizzy::bytes_view Wasm3Engine::get_memory() const
{
    uint32_t size;
    auto data = m3_GetMemory(m_runtime, &size, 0);
    if (data == nullptr)
        return {};
    return {data, size};
}

std::optional<WasmEngine::FuncRef> Wasm3Engine::find_function(
    std::string_view name, std::string_view signature) const
{
    IM3Function function;
    if (m3_FindFunction(&function, m_runtime, name.data()) != m3Err_none)
        return std::nullopt;

    const auto [inputs, outputs] = translate_function_signature<M3ValueType,
        M3ValueType::c_m3Type_i32, M3ValueType::c_m3Type_i64>(signature);

    if (inputs.size() != m3_GetArgCount(function))
        return std::nullopt;
    for (unsigned i = 0; i < m3_GetArgCount(function); i++)
        if (inputs[i] != m3_GetArgType(function, i))
            return std::nullopt;

    if (outputs.size() != m3_GetRetCount(function))
        return std::nullopt;
    for (unsigned i = 0; i < m3_GetRetCount(function); i++)
        if (outputs[i] != m3_GetRetType(function, i))
            return std::nullopt;

    return reinterpret_cast<WasmEngine::FuncRef>(function);
}

WasmEngine::Result Wasm3Engine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    auto function = reinterpret_cast<IM3Function>(func_ref);  // NOLINT(performance-no-int-to-ptr)

    if (args.size() > 32)
        throw std::runtime_error{"Does not support more than 32 arguments"};
    const void* argPtrs[32];
    for (unsigned i = 0; i < args.size(); i++)
        argPtrs[i] = &args[i];

    // This ensures input count/type matches. For the return value we assume find_function did the
    // validation.
    if (m3_Call(function, static_cast<uint32_t>(args.size()), argPtrs) == m3Err_none)
    {
        if (m3_GetRetCount(function) == 0)
            return {false, std::nullopt};

        uint64_t ret_value = 0;
        // This should not fail because we check GetRetCount.
        [[maybe_unused]] const auto ret = m3_GetResultsV(function, &ret_value);
        assert(ret == m3Err_none);
        return {false, ret_value};
    }
    return {true, std::nullopt};
}
}  // namespace fizzy::test
