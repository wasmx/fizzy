// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "source/wasm3.h"

#include <test/utils/adler32.hpp>
#include <test/utils/wasm_engine.hpp>
#include <cassert>
#include <cstring>

namespace wasm3_bigint
{
#define BIGINT_BITS 384
#define LIMB_BITS 64
#define LIMB_BITS_OVERFLOW 128
#include <test/utils/bigint.h>
#undef BIGINT_BITS
#undef LIMB_BITS
#undef LIMB_BITS_OVERFLOW
}  // namespace wasm3_bigint

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
const void* env_adler32(IM3Runtime /*runtime*/, uint64_t* stack, void* mem)
{
    uint64_t* ret = stack;
    const uint32_t offset = *(uint32_t*)(stack++);
    const uint32_t length = *(uint32_t*)(stack++);
    *ret = fizzy::adler32({reinterpret_cast<uint8_t*>(mem) + offset, length});
    return m3Err_none;
}

const uint64_t mod[] = {0xb9feffffffffaaab, 0x1eabfffeb153ffff, 0x6730d2a0f6b0f624,
    0x64774b84f38512bf, 0x4b1ba7b6434bacd7, 0x1a0111ea397fe69a};
const uint64_t modinv = 0x89f3fffcfffcfffd;

const void* bignum_int_add(IM3Runtime /*runtime*/, uint64_t* stack, void* mem)
{
    uint64_t* ret = stack;
    const uint32_t a_offset = *(uint32_t*)(stack++);
    const uint32_t b_offset = *(uint32_t*)(stack++);
    const uint32_t ret_offset = *(uint32_t*)(stack++);
    const uint64_t* a = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + a_offset);
    const uint64_t* b = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + b_offset);
    uint64_t* out = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + ret_offset);
    *ret = wasm3_bigint::add384_64bitlimbs(out, a, b);
    return m3Err_none;
}
const void* bignum_int_sub(IM3Runtime /*runtime*/, uint64_t* stack, void* mem)
{
    uint64_t* ret = stack;
    const uint32_t a_offset = *(uint32_t*)(stack++);
    const uint32_t b_offset = *(uint32_t*)(stack++);
    const uint32_t ret_offset = *(uint32_t*)(stack++);
    const uint64_t* a = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + a_offset);
    const uint64_t* b = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + b_offset);
    uint64_t* out = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + ret_offset);
    *ret = wasm3_bigint::sub384_64bitlimbs(out, a, b);
    return m3Err_none;
}
const void* bignum_int_mul(IM3Runtime /*runtime*/, uint64_t* stack, void* mem)
{
    const uint32_t a_offset = *(uint32_t*)(stack++);
    const uint32_t b_offset = *(uint32_t*)(stack++);
    const uint32_t ret_offset = *(uint32_t*)(stack++);
    const uint64_t* a = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + a_offset);
    const uint64_t* b = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + b_offset);
    uint64_t* out = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + ret_offset);
    wasm3_bigint::mul384_64bitlimbs(out, a, b);
    return m3Err_none;
}
const void* bignum_int_div(IM3Runtime /*runtime*/, uint64_t* stack, void* mem)
{
    const uint32_t a_offset = *(uint32_t*)(stack++);
    const uint32_t b_offset = *(uint32_t*)(stack++);
    const uint32_t q_offset = *(uint32_t*)(stack++);
    const uint32_t r_offset = *(uint32_t*)(stack++);
    const uint64_t* a = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + a_offset);
    const uint64_t* b = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + b_offset);
    uint64_t* q = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + q_offset);
    uint64_t* r = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + r_offset);
    wasm3_bigint::div384_64bitlimbs(q, r, a, b);
    return m3Err_none;
}
const void* bignum_f1m_add(IM3Runtime /*runtime*/, uint64_t* stack, void* mem)
{
    const uint32_t a_offset = *(uint32_t*)(stack++);
    const uint32_t b_offset = *(uint32_t*)(stack++);
    const uint32_t ret_offset = *(uint32_t*)(stack++);
    const uint64_t* a = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + a_offset);
    const uint64_t* b = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + b_offset);
    uint64_t* out = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + ret_offset);
    wasm3_bigint::addmod384_64bitlimbs(out, a, b, mod);
    return m3Err_none;
}
const void* bignum_f1m_sub(IM3Runtime /*runtime*/, uint64_t* stack, void* mem)
{
    const uint32_t a_offset = *(uint32_t*)(stack++);
    const uint32_t b_offset = *(uint32_t*)(stack++);
    const uint32_t ret_offset = *(uint32_t*)(stack++);
    const uint64_t* a = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + a_offset);
    const uint64_t* b = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + b_offset);
    uint64_t* out = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + ret_offset);
    wasm3_bigint::submod384_64bitlimbs(out, a, b, mod);
    return m3Err_none;
}
const void* bignum_f1m_mul(IM3Runtime /*runtime*/, uint64_t* stack, void* mem)
{
    const uint32_t a_offset = *(uint32_t*)(stack++);
    const uint32_t b_offset = *(uint32_t*)(stack++);
    const uint32_t ret_offset = *(uint32_t*)(stack++);
    const uint64_t* a = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + a_offset);
    const uint64_t* b = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + b_offset);
    uint64_t* out = reinterpret_cast<uint64_t*>(reinterpret_cast<uint8_t*>(mem) + ret_offset);
    wasm3_bigint::mulmodmont384_64bitlimbs(out, a, b, mod, modinv);
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

    auto ret = m3_LinkRawFunction(module, "env", "adler32", "i(ii)", env_adler32);
    if (ret != m3Err_none && ret != m3Err_functionLookupFailed)
    {
        m3_FreeRuntime(m_runtime);
        return false;
    }
    m3_LinkRawFunction(module, "env", "bignum_int_add", "i(iii)", bignum_int_add);
    if (ret != m3Err_none && ret != m3Err_functionLookupFailed)
    {
        m3_FreeRuntime(m_runtime);
        return false;
    }
    m3_LinkRawFunction(module, "env", "bignum_int_sub", "i(iii)", bignum_int_sub);
    if (ret != m3Err_none && ret != m3Err_functionLookupFailed)
    {
        m3_FreeRuntime(m_runtime);
        return false;
    }
    m3_LinkRawFunction(module, "env", "bignum_int_mul", "v(iii)", bignum_int_mul);
    if (ret != m3Err_none && ret != m3Err_functionLookupFailed)
    {
        m3_FreeRuntime(m_runtime);
        return false;
    }
    m3_LinkRawFunction(module, "env", "bignum_int_div", "v(iiii)", bignum_int_div);
    if (ret != m3Err_none && ret != m3Err_functionLookupFailed)
    {
        m3_FreeRuntime(m_runtime);
        return false;
    }
    m3_LinkRawFunction(module, "env", "bignum_f1m_add", "v(iii)", bignum_f1m_add);
    if (ret != m3Err_none && ret != m3Err_functionLookupFailed)
    {
        m3_FreeRuntime(m_runtime);
        return false;
    }
    m3_LinkRawFunction(module, "env", "bignum_f1m_sub", "v(iii)", bignum_f1m_sub);
    if (ret != m3Err_none && ret != m3Err_functionLookupFailed)
    {
        m3_FreeRuntime(m_runtime);
        return false;
    }
    m3_LinkRawFunction(module, "env", "bignum_f1m_mul", "v(iii)", bignum_f1m_mul);
    if (ret != m3Err_none && ret != m3Err_functionLookupFailed)
    {
        m3_FreeRuntime(m_runtime);
        return false;
    }

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

std::optional<WasmEngine::FuncRef> Wasm3Engine::find_function(
    std::string_view name, std::string_view) const
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
