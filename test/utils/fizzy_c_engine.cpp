// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>

#include <test/utils/adler32.hpp>
#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>

namespace fizzy::test
{
class FizzyCEngine final : public WasmEngine
{
    std::unique_ptr<FizzyInstance, void (*)(FizzyInstance*)> m_instance = {
        nullptr, fizzy_free_instance};

public:
    bool parse(bytes_view input) const final;
    std::optional<FuncRef> find_function(
        std::string_view name, std::string_view signature) const final;
    bool instantiate(bytes_view wasm_binary) final;
    bool init_memory(bytes_view memory) final;
    bytes_view get_memory() const final;
    Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) final;
};

namespace
{
FizzyExecutionResult env_adler32(
    void*, FizzyInstance* instance, const FizzyValue* args, FizzyExecutionContext*) noexcept
{
    try
    {
        auto* memory = fizzy_get_instance_memory_data(instance);
        assert(memory != nullptr);
        const auto size = fizzy_get_instance_memory_size(instance);
        const auto ret = fizzy::test::adler32(
            bytes_view(memory, size)
                .substr(static_cast<uint32_t>(args[0].i64), static_cast<uint32_t>(args[1].i64)));
        return {false, true, {ret}};
    }
    catch (...)
    {
        return {true, false, {}};
    }
}
}  // namespace

std::unique_ptr<WasmEngine> create_fizzy_c_engine()
{
    return std::make_unique<FizzyCEngine>();
}

bool FizzyCEngine::parse(bytes_view input) const
{
    const auto module = fizzy_parse(input.data(), input.size(), nullptr);
    if (!module)
        return false;

    fizzy_free_module(module);
    return true;
}

bool FizzyCEngine::instantiate(bytes_view wasm_binary)
{
    const auto module = fizzy_parse(wasm_binary.data(), wasm_binary.size(), nullptr);
    if (!module)
        return false;

    FizzyValueType inputs[] = {FizzyValueTypeI32, FizzyValueTypeI32};
    FizzyImportedFunction imports[] = {
        {"env", "adler32", {{FizzyValueTypeI32, inputs, 2}, env_adler32, nullptr}}};
    m_instance.reset(fizzy_resolve_instantiate(
        module, imports, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr));

    return (m_instance != nullptr);
}

bool FizzyCEngine::init_memory(bytes_view memory)
{
    auto* instance_memory = fizzy_get_instance_memory_data(m_instance.get());
    if (instance_memory == nullptr)
        return false;

    const auto size = fizzy_get_instance_memory_size(m_instance.get());
    if (size < memory.size())
        return false;

    std::memcpy(instance_memory, memory.data(), memory.size());
    return true;
}

bytes_view FizzyCEngine::get_memory() const
{
    auto* data = fizzy_get_instance_memory_data(m_instance.get());
    if (data == nullptr)
        return {};

    const auto size = fizzy_get_instance_memory_size(m_instance.get());
    return {data, size};
}

std::optional<WasmEngine::FuncRef> FizzyCEngine::find_function(
    std::string_view name, std::string_view /*signature*/) const
{
    uint32_t func_idx;
    if (!fizzy_find_exported_function_index(
            fizzy_get_instance_module(m_instance.get()), std::string{name}.c_str(), &func_idx))
        return std::nullopt;

    return func_idx;
}

WasmEngine::Result FizzyCEngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    static_assert(sizeof(uint64_t) == sizeof(FizzyValue));
    const auto func_idx = static_cast<uint32_t>(func_ref);
    const auto* module = fizzy_get_instance_module(m_instance.get());
    const auto func_type = fizzy_get_function_type(module, func_idx);
    assert(args.size() == func_type.inputs_size);
    assert(func_type.output != FizzyValueTypeF32 && func_type.output != FizzyValueTypeF64 &&
           "floating point result types are not supported");
    const auto first_arg = reinterpret_cast<const FizzyValue*>(args.data());
    const auto status = fizzy_execute(m_instance.get(), func_idx, first_arg, nullptr);
    if (status.trapped)
        return {true, std::nullopt};
    else if (status.has_value)
        return {false, func_type.output == FizzyValueTypeI32 ? status.value.i32 : status.value.i64};
    else
        return {false, std::nullopt};
}
}  // namespace fizzy::test
