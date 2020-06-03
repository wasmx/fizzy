// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <src/binary-reader.h>
#include <src/interp/binary-reader-interp.h>
#include <src/interp/interp.h>

#include <test/utils/adler32.hpp>
#include <test/utils/wasm_engine.hpp>
#include <cassert>

namespace wabt_bigint
{
#define BIGINT_BITS 384
#define LIMB_BITS 64
#define LIMB_BITS_OVERFLOW 128
#include <test/utils/bigint.h>
#undef BIGINT_BITS
#undef LIMB_BITS
#undef LIMB_BITS_OVERFLOW
}  // namespace wabt_bigint

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
    hostModule->AppendFuncExport("bignum_int_add",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {wabt::Type::I32}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues&, wabt::interp::TypedValues&) {
            assert(false);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_int_sub",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {wabt::Type::I32}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues&, wabt::interp::TypedValues&) {
            assert(false);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_int_mul",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues&, wabt::interp::TypedValues&) {
            assert(false);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_int_div",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues&, wabt::interp::TypedValues&) {
            assert(false);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_f1m_add",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues&, wabt::interp::TypedValues&) {
            assert(false);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_f1m_sub",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues&, wabt::interp::TypedValues&) {
            assert(false);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_f1m_mul",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
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
                fizzy::adler32({reinterpret_cast<uint8_t*>(&memory_data[offset]), length});
            results[0].set_i32(ret);
            return wabt::interp::Result::Ok;
        });

    const uint64_t mod[] = {0xb9feffffffffaaab, 0x1eabfffeb153ffff, 0x6730d2a0f6b0f624,
        0x64774b84f38512bf, 0x4b1ba7b6434bacd7, 0x1a0111ea397fe69a};
    const uint64_t modinv = 0x89f3fffcfffcfffd;

    hostModule->AppendFuncExport("bignum_int_add",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {wabt::Type::I32}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues& args, wabt::interp::TypedValues& results) {
            auto memory = m_env.GetMemory(0);
            auto memory_data = memory->data;
            const auto a_offset = args[0].value.i32;
            const auto b_offset = args[1].value.i32;
            const auto ret_offset = args[2].value.i32;
            const uint64_t* a = reinterpret_cast<uint64_t*>(&memory_data[a_offset]);
            const uint64_t* b = reinterpret_cast<uint64_t*>(&memory_data[b_offset]);
            uint64_t* out = reinterpret_cast<uint64_t*>(&memory_data[ret_offset]);
            const auto ret = wabt_bigint::add384_64bitlimbs(out, a, b);
            results[0].set_i32(static_cast<uint32_t>(ret));
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_int_sub",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {wabt::Type::I32}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues& args, wabt::interp::TypedValues& results) {
            auto memory = m_env.GetMemory(0);
            auto memory_data = memory->data;
            const auto a_offset = args[0].value.i32;
            const auto b_offset = args[1].value.i32;
            const auto ret_offset = args[2].value.i32;
            const uint64_t* a = reinterpret_cast<uint64_t*>(&memory_data[a_offset]);
            const uint64_t* b = reinterpret_cast<uint64_t*>(&memory_data[b_offset]);
            uint64_t* out = reinterpret_cast<uint64_t*>(&memory_data[ret_offset]);
            const auto ret = wabt_bigint::sub384_64bitlimbs(out, a, b);
            results[0].set_i32(static_cast<uint32_t>(ret));
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_int_mul",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues& args, wabt::interp::TypedValues&) {
            auto memory = m_env.GetMemory(0);
            auto memory_data = memory->data;
            const auto a_offset = args[0].value.i32;
            const auto b_offset = args[1].value.i32;
            const auto ret_offset = args[2].value.i32;
            const uint64_t* a = reinterpret_cast<uint64_t*>(&memory_data[a_offset]);
            const uint64_t* b = reinterpret_cast<uint64_t*>(&memory_data[b_offset]);
            uint64_t* out = reinterpret_cast<uint64_t*>(&memory_data[ret_offset]);
            wabt_bigint::mul384_64bitlimbs(out, a, b);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_int_div",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues& args, wabt::interp::TypedValues&) {
            auto memory = m_env.GetMemory(0);
            auto memory_data = memory->data;
            const auto a_offset = args[0].value.i32;
            const auto b_offset = args[1].value.i32;
            const auto q_offset = args[2].value.i32;
            const auto r_offset = args[3].value.i32;
            const uint64_t* a = reinterpret_cast<uint64_t*>(&memory_data[a_offset]);
            const uint64_t* b = reinterpret_cast<uint64_t*>(&memory_data[b_offset]);
            uint64_t* q = reinterpret_cast<uint64_t*>(&memory_data[q_offset]);
            uint64_t* r = reinterpret_cast<uint64_t*>(&memory_data[r_offset]);
            wabt_bigint::div384_64bitlimbs(q, r, a, b);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_f1m_add",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues& args, wabt::interp::TypedValues&) {
            auto memory = m_env.GetMemory(0);
            auto memory_data = memory->data;
            const auto a_offset = args[0].value.i32;
            const auto b_offset = args[1].value.i32;
            const auto ret_offset = args[2].value.i32;
            const uint64_t* a = reinterpret_cast<uint64_t*>(&memory_data[a_offset]);
            const uint64_t* b = reinterpret_cast<uint64_t*>(&memory_data[b_offset]);
            uint64_t* out = reinterpret_cast<uint64_t*>(&memory_data[ret_offset]);
            wabt_bigint::addmod384_64bitlimbs(out, a, b, mod);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_f1m_sub",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues& args, wabt::interp::TypedValues&) {
            auto memory = m_env.GetMemory(0);
            auto memory_data = memory->data;
            const auto a_offset = args[0].value.i32;
            const auto b_offset = args[1].value.i32;
            const auto ret_offset = args[2].value.i32;
            const uint64_t* a = reinterpret_cast<uint64_t*>(&memory_data[a_offset]);
            const uint64_t* b = reinterpret_cast<uint64_t*>(&memory_data[b_offset]);
            uint64_t* out = reinterpret_cast<uint64_t*>(&memory_data[ret_offset]);
            wabt_bigint::submod384_64bitlimbs(out, a, b, mod);
            return wabt::interp::Result::Ok;
        });
    hostModule->AppendFuncExport("bignum_f1m_mul",
        {{wabt::Type::I32, wabt::Type::I32, wabt::Type::I32}, {}},
        [=](const wabt::interp::HostFunc*, const wabt::interp::FuncSignature*,
            const wabt::interp::TypedValues& args, wabt::interp::TypedValues&) {
            auto memory = m_env.GetMemory(0);
            auto memory_data = memory->data;
            const auto a_offset = args[0].value.i32;
            const auto b_offset = args[1].value.i32;
            const auto ret_offset = args[2].value.i32;
            const uint64_t* a = reinterpret_cast<uint64_t*>(&memory_data[a_offset]);
            const uint64_t* b = reinterpret_cast<uint64_t*>(&memory_data[b_offset]);
            uint64_t* out = reinterpret_cast<uint64_t*>(&memory_data[ret_offset]);
            wabt_bigint::mulmodmont384_64bitlimbs(out, a, b, mod, modinv);
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
    if (r.result != wabt::interp::Result::Ok)
        return false;

    return true;
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
