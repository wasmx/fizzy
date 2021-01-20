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
class FizzyEngine final : public WasmEngine
{
    std::unique_ptr<Instance> m_instance;

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
ValType translate_valtype(char input)
{
    if (input == 'i')
        return fizzy::ValType::i32;
    else if (input == 'I')
        return fizzy::ValType::i64;
    else
        throw std::runtime_error{"invalid type"};
}

FuncType translate_signature(std::string_view signature)
{
    const auto delimiter_pos = signature.find(':');
    assert(delimiter_pos != std::string_view::npos);
    const auto inputs = signature.substr(0, delimiter_pos);
    const auto outputs = signature.substr(delimiter_pos + 1);

    FuncType func_type;
    std::transform(std::begin(inputs), std::end(inputs), std::back_inserter(func_type.inputs),
        translate_valtype);
    std::transform(std::begin(outputs), std::end(outputs), std::back_inserter(func_type.outputs),
        translate_valtype);
    return func_type;
}

fizzy::ExecutionResult env_adler32(fizzy::Instance& instance, const fizzy::Value* args, int)
{
    assert(instance.memory != nullptr);
    const auto ret = fizzy::test::adler32(
        bytes_view{*instance.memory}.substr(args[0].as<uint32_t>(), args[1].as<uint32_t>()));
    return Value{ret};
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
        auto module = fizzy::parse(wasm_binary);
        auto imports = fizzy::resolve_imported_functions(
            *module, {
                         {"env", "adler32", {fizzy::ValType::i32, fizzy::ValType::i32},
                             fizzy::ValType::i32, env_adler32},
                     });
        m_instance = fizzy::instantiate(std::move(module), std::move(imports));
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

std::optional<WasmEngine::FuncRef> FizzyEngine::find_function(
    std::string_view name, std::string_view signature) const
{
    const auto func_idx = fizzy::find_exported_function(*m_instance->module, name);
    if (func_idx.has_value())
    {
        const auto func_type = m_instance->module->get_function_type(*func_idx);
        const auto sig_type = translate_signature(signature);
        if (sig_type != func_type)
            return std::nullopt;
    }
    return func_idx;
}

WasmEngine::Result FizzyEngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    static_assert(sizeof(uint64_t) == sizeof(Value));
    const auto first_arg = reinterpret_cast<const Value*>(args.data());
    const auto& func_type = m_instance->module->get_function_type(static_cast<uint32_t>(func_ref));
    assert(args.size() == func_type.inputs.size());
    const auto status = fizzy::execute(*m_instance, static_cast<uint32_t>(func_ref), first_arg);
    if (status.trapped)
        return {true, std::nullopt};
    else if (status.has_value)
    {
        assert(func_type.outputs[0] != ValType::f32 && func_type.outputs[0] != ValType::f64 &&
               "floating point result types are not supported");
        return {false, func_type.outputs[0] == ValType::i32 ? status.value.i32 : status.value.i64};
    }
    else
        return {false, std::nullopt};
}
}  // namespace fizzy::test
