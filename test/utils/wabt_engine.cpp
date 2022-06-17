// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <src/binary-reader.h>
#include <src/interp/binary-reader-interp.h>
#include <src/interp/interp.h>

#include <test/utils/adler32.hpp>
#include <test/utils/wasm_engine.hpp>
#include <algorithm>
#include <cassert>

namespace fizzy::test
{
class WabtEngine final : public WasmEngine
{
    // mutable because getting RefPtr from Ref requires non-const Store
    mutable wabt::interp::Store m_store{([]() constexpr noexcept {
        wabt::Features features;
        features.disable_multi_value();
        features.disable_sat_float_to_int();
        features.disable_sign_extension();
        return features;
    })()};
    wabt::interp::Module::Ptr m_module;
    wabt::interp::Instance::Ptr m_instance;
    wabt::interp::Thread::Ptr m_thread{wabt::interp::Thread::New(m_store, {})};

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
    wabt::interp::ModuleDesc module_desc;
    wabt::Errors errors;
    const wabt::Result result = wabt::interp::ReadBinaryInterp(
        input.data(), input.size(), wabt::ReadBinaryOptions{}, &errors, &module_desc);
    return (result == wabt::Result::Ok);
}

bool WabtEngine::instantiate(bytes_view wasm_binary)
{
    wabt::Errors errors;
    wabt::interp::ModuleDesc module_desc;
    const wabt::Result result = wabt::interp::ReadBinaryInterp(
        wasm_binary.data(), wasm_binary.size(), wabt::ReadBinaryOptions{}, &errors, &module_desc);
    if (result != wabt::Result::Ok)
        return false;

    m_module = wabt::interp::Module::New(m_store, module_desc);
    assert(m_module);

    const auto host_func = wabt::interp::HostFunc::New(m_store,
        wabt::interp::FuncType{{wabt::Type::I32, wabt::Type::I32}, {wabt::Type::I32}},
        [](wabt::interp::Thread& thread, const wabt::interp::Values& args,
            wabt::interp::Values& results, wabt::interp::Trap::Ptr*) noexcept -> wabt::Result {
            assert(args.size() == 2);
            const auto offset = args[0].i32_;
            const auto length = args[1].i32_;
            // TODO: move the memory lookup outside in order to reduce overhead
            assert(!thread.GetCallerInstance()->memories().empty());
            wabt::interp::Memory::Ptr memory(
                thread.store(), thread.GetCallerInstance()->memories()[0]);
            assert(memory);
            auto memory_data = memory->UnsafeData();
            assert(memory->ByteSize() > (offset + length));
            const auto ret =
                fizzy::test::adler32({reinterpret_cast<uint8_t*>(&memory_data[offset]), length});
            assert(results.size() == 1);
            results[0].Set(ret);
            return wabt::Result::Ok;
        });
    assert(host_func);
    const wabt::interp::RefVec imports = wabt::interp::RefVec{host_func->self()};

    wabt::interp::Trap::Ptr trap;
    m_instance = wabt::interp::Instance::Instantiate(m_store, m_module.ref(), imports, &trap);

    return m_instance && !trap;
}

bool WabtEngine::init_memory(bytes_view memory)
{
    assert(m_instance);
    if (m_instance->memories().empty())
        return false;

    wabt::interp::Memory::Ptr dst_memory(m_store, m_instance->memories()[0]);
    assert(dst_memory);

    if (dst_memory->ByteSize() < memory.size())
        return false;

    std::memcpy(dst_memory->UnsafeData(), memory.data(), memory.size());
    return true;
}

bytes_view WabtEngine::get_memory() const
{
    assert(m_instance);
    if (m_instance->memories().empty())
        return {};

    wabt::interp::Memory::Ptr memory(m_store, m_instance->memories()[0]);
    assert(memory);

    return {memory->UnsafeData(), memory->ByteSize()};
}

std::optional<WasmEngine::FuncRef> WabtEngine::find_function(
    std::string_view name, std::string_view) const
{
    assert(m_module);
    const auto& exports = m_module->desc().exports;
    const auto it_export =
        std::find_if(exports.begin(), exports.end(), [name](const wabt::interp::ExportDesc& e) {
            return (e.type.type->kind == wabt::ExternalKind::Func && e.type.name == name);
        });
    if (it_export == exports.end())
        return {};

    return WasmEngine::FuncRef{it_export->index};
}

WasmEngine::Result WabtEngine::execute(
    WasmEngine::FuncRef func_ref, const std::vector<uint64_t>& args)
{
    assert(m_instance);
    assert(m_thread);

    const auto func_idx = static_cast<wabt::interp::Index>(func_ref);

    assert(func_idx < m_instance->funcs().size());
    const auto func = m_store.UnsafeGet<wabt::interp::DefinedFunc>(m_instance->funcs()[func_idx]);
    assert(func);

    wabt::interp::Values typed_args;
    for (size_t i = 0; i < args.size(); ++i)
    {
        wabt::interp::Value value{};
        const auto type = func->type().params[i];
        if (type == wabt::Type::I32)
            value.Set(static_cast<uint32_t>(args[i]));
        else
            value.Set(args[i]);
        typed_args.push_back(value);
    }

    wabt::interp::Values typed_results;
    if (!func->type().results.empty())
        typed_results.push_back({});

    wabt::interp::Trap::Ptr trap;
    const auto call_result = func->Call(*m_thread, typed_args, typed_results, &trap);

    if (call_result != wabt::Result::Ok || trap)
        return {true, {}};

    WasmEngine::Result result{false, {}};
    if (!typed_results.empty())
    {
        const auto type = func->type().results[0];
        const auto& value = typed_results[0];
        if (type == wabt::Type::I32)
            result.value = value.i32_;
        else if (type == wabt::Type::I64)
            result.value = value.i64_;
    }
    return result;
}
}  // namespace fizzy::test
