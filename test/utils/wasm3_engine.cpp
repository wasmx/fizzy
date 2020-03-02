#include "source/wasm3.h"

#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>

namespace fizzy::test
{
static_assert(sizeof(IM3Function) <= sizeof(WasmEngine::FuncRef));

class Wasm3Engine : public WasmEngine
{
    IM3Environment m_env{nullptr};
    IM3Runtime m_runtime{nullptr};

public:
    Wasm3Engine() : m_env{m3_NewEnvironment()} {}
    ~Wasm3Engine()
    {
        if (m_runtime)
            m3_FreeRuntime(m_runtime);
        m3_FreeEnvironment(m_env);
    }

    bool parse(bytes_view input) final;
    std::optional<FuncRef> find_function(std::string_view name) const final;
    bool instantiate() final;
    bool init_memory(fizzy::bytes_view memory) final;
    fizzy::bytes_view get_memory() const final;
    Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) final;
};

std::unique_ptr<WasmEngine> create_wasm3_engine()
{
    return std::make_unique<Wasm3Engine>();
}

bool Wasm3Engine::parse(bytes_view input)
{
    // Replace runtime (e.g. instance + module)
    if (m_runtime != nullptr)
        m3_FreeRuntime(m_runtime);

    // The 64k stack size comes from wasm3/platorms/app
    m_runtime = m3_NewRuntime(m_env, 64 * 1024, nullptr);
    if (m_runtime == nullptr)
        return false;

    IM3Module module;
    if (m3_ParseModule(m_env, &module, input.data(), uint32_t(input.size())) != m3Err_none)
        return false;

    // Transfers ownership to runtime.
    if (m3_LoadModule(m_runtime, module) != m3Err_none)
    {
        m3_FreeModule(module);
        return false;
    }

    return true;
}

bool Wasm3Engine::instantiate()
{
    // Already done in parse (with owernship transfer).
    return true;
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

std::optional<WasmEngine::FuncRef> Wasm3Engine::find_function(std::string_view name) const
{
    IM3Function function;
    if (m3_FindFunction(&function, m_runtime, name.data()) == m3Err_none)
        return reinterpret_cast<WasmEngine::FuncRef>(function);
    return std::nullopt;
}

WasmEngine::Result Wasm3Engine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    unsigned ret_valid;
    uint64_t ret_value;
    IM3Function function = reinterpret_cast<IM3Function>(func_ref);
    auto const result = m3_CallProper(
        function, static_cast<uint32_t>(args.size()), args.data(), &ret_valid, &ret_value);
    if (result == m3Err_none)
        return {false, ret_valid ? ret_value : std::optional<uint64_t>{}};
    return {true, std::nullopt};
}
}  // namespace fizzy::test
