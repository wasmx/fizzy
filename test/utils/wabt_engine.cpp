// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <src/binary-reader.h>
#include <src/interp/binary-reader-interp.h>
#include <src/interp/interp.h>

#include <test/utils/adler32.hpp>
#include <test/utils/wasm_engine.hpp>
#include <cassert>

namespace fizzy::test
{
class WabtEngine : public WasmEngine
{
    wabt::interp::Environment m_env;
    wabt::interp::DefinedModule* m_module{nullptr};

    // WABT Executor/Thread with default options.
    wabt::interp::Executor m_executor{&m_env};

public:
    bool parse(bytes_view input) const final;
    std::optional<FuncRef> find_function(
        std::string_view name, std::string_view signature) const final;
    bool instantiate(bytes_view wasm_binary) final;
    bool init_memory(fizzy::bytes_view memory) final;
    bytes_view get_memory() const final;
    Result execute(FuncRef func_ref, const std::vector<uint64_t>& args) final;
};

std::unique_ptr<WasmEngine> create_wabt_engine()
{
    return std::make_unique<WabtEngine>();
}

bool WabtEngine::parse(bytes_view input) const
{
    wabt::interp::Environment env;

    wabt::interp::HostModule* hostModule = env.AppendHostModule("env");
    assert(hostModule != nullptr);

    hostModule->AppendFuncExport("adler32", {{wabt::Type::I32, wabt::Type::I32}, {wabt::Type::I32}},
        [](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues&, wabt::interp::TypedValues&) {
            assert(false);
            return wabt::interp::Result::Ok;
        });

    wabt::interp::DefinedModule* module{nullptr};
    wabt::Errors errors;
    const wabt::Result result = wabt::ReadBinaryInterp(
        &env, input.data(), input.size(), wabt::ReadBinaryOptions{}, &errors, &module);
    return (result == wabt::Result::Ok);
}

bool WabtEngine::instantiate(bytes_view wasm_binary)
{
    wabt::interp::HostModule* hostModule = m_env.AppendHostModule("env");
    assert(hostModule != nullptr);

    hostModule->AppendFuncExport("adler32", {{wabt::Type::I32, wabt::Type::I32}, {wabt::Type::I32}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues& args, wabt::interp::TypedValues& results) {
            const auto offset = args[0].value.i32;
            const auto length = args[1].value.i32;
            // TODO: move the memory lookup outside in order to reduce overhead
            assert(m_env.GetMemoryCount() > 0);
            auto memory = m_env.GetMemory(0);
            assert(memory != nullptr);
            auto memory_data = memory->data;
            assert(memory_data.size() > (offset + length));
            const auto ret =
                fizzy::test::adler32({reinterpret_cast<uint8_t*>(&memory_data[offset]), length});
            results[0].set_i32(ret);
            return wabt::interp::Result::Ok;
        });

    wabt::Errors errors;
    const wabt::Result result = wabt::ReadBinaryInterp(&m_env, wasm_binary.data(),
        wasm_binary.size(), wabt::ReadBinaryOptions{}, &errors, &m_module);
    if (result != wabt::Result::Ok)
        return false;
    assert(m_module != nullptr);

    // Run start function (this will return ok if no start function is present)
    const wabt::interp::ExecResult r = m_executor.RunStartFunction(m_module);
    return r.result == wabt::interp::Result::Ok;
}

bool WabtEngine::init_memory(bytes_view memory)
{
    if (m_env.GetMemoryCount() == 0)
        return false;

    auto& dst = *m_env.GetMemory(0);
    if (dst.data.size() < memory.size())
        return false;

    std::memcpy(dst.data.data(), memory.data(), memory.size());
    return true;
}

bytes_view WabtEngine::get_memory() const
{
    if (m_env.GetMemoryCount() == 0)
        return {};

    auto& env = const_cast<wabt::interp::Environment&>(m_env);
    const auto& memory = env.GetMemory(0);
    return {reinterpret_cast<uint8_t*>(memory->data.data()), memory->data.size()};
}

std::optional<WasmEngine::FuncRef> WabtEngine::find_function(
    std::string_view name, std::string_view) const
{
    const wabt::interp::Export* e = m_module->GetExport({name.data(), name.size()});
    if (e != nullptr && e->kind == wabt::ExternalKind::Func)
        return reinterpret_cast<WasmEngine::FuncRef>(e);
    return {};
}

WasmEngine::Result WabtEngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    const auto* e = reinterpret_cast<const wabt::interp::Export*>(func_ref);

    const auto func_sig = m_env.GetFuncSignature(m_env.GetFunc(e->index)->sig_index);
    assert(func_sig->param_types.size() == args.size());

    wabt::interp::TypedValues typed_args;
    for (size_t i = 0; i < args.size(); ++i)
    {
        wabt::interp::Value value{};
        const auto type = func_sig->param_types[i];
        if (type == wabt::Type::I32)
            value.i32 = static_cast<uint32_t>(args[i]);
        else
            value.i64 = args[i];
        typed_args.push_back(wabt::interp::TypedValue{type, value});
    }
    wabt::interp::ExecResult r = m_executor.RunExport(e, typed_args);

    if (r.result != wabt::interp::Result::Ok)
        return {true, {}};

    WasmEngine::Result result{false, {}};
    if (!r.values.empty())
    {
        const auto& value = r.values[0];
        if (value.type == wabt::Type::I32)
            result.value = value.get_i32();
        else if (value.type == wabt::Type::I64)
            result.value = value.get_i64();
    }
    return result;
}
}  // namespace fizzy::test
