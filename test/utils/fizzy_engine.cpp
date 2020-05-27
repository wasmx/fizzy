// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"

#include <test/utils/adler32.hpp>
#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>

namespace fizzy::test
{
class FizzyEngine : public WasmEngine
{
    std::unique_ptr<Instance> m_instance;

public:
    bool parse(bytes_view input) const final;
    std::optional<FuncRef> find_function(std::string_view name) const final;
    bool instantiate(bytes_view wasm_binary) final;
    bool init_memory(bytes_view memory) final;
    bytes_view get_memory() const final;
    Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) final;
};

namespace
{
fizzy::execution_result env_adler32(
    fizzy::Instance& instance, uint64_t* args, [[maybe_unused]] size_t num_args, int)
{
    assert(instance.memory != nullptr);
    assert(num_args == 2);
    const auto ret = fizzy::adler32(bytes_view{*instance.memory}.substr(args[0], args[1]));
    return {false, {ret}};
}
}  // namespace

std::unique_ptr<WasmEngine> create_fizzy_engine()
{
    return std::make_unique<FizzyEngine>();
}

bool FizzyEngine::parse(bytes_view input) const
{
    try
    {
        fizzy::parse(input);
    }
    catch (...)
    {
        return false;
    }
    return true;
}

bool FizzyEngine::instantiate(bytes_view wasm_binary)
{
    try
    {
        const auto module = fizzy::parse(wasm_binary);
        auto imports = fizzy::resolve_imported_functions(
            module, {
                        {"env", "adler32", {fizzy::ValType::i32, fizzy::ValType::i32},
                            fizzy::ValType::i32, env_adler32},
                    });
        m_instance = fizzy::instantiate(module, imports);
    }
    catch (...)
    {
        return false;
    }
    assert(m_instance != nullptr);
    return true;
}

bool FizzyEngine::init_memory(bytes_view memory)
{
    if (m_instance->memory == nullptr || m_instance->memory->size() < memory.size())
        return false;

    std::memcpy(m_instance->memory->data(), memory.data(), memory.size());
    return true;
}

bytes_view FizzyEngine::get_memory() const
{
    if (!m_instance->memory)
        return {};

    return {m_instance->memory->data(), m_instance->memory->size()};
}

std::optional<WasmEngine::FuncRef> FizzyEngine::find_function(std::string_view name) const
{
    return fizzy::find_exported_function(m_instance->module, name);
}

WasmEngine::Result FizzyEngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    const auto [trapped, result_stack] =
        fizzy::execute(*m_instance, static_cast<uint32_t>(func_ref), args);
    assert(result_stack.size() <= 1);
    return {trapped, !result_stack.empty() ? result_stack.back() : std::optional<uint64_t>{}};
}
}  // namespace fizzy::test
