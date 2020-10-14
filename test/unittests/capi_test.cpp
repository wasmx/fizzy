// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy::test;

TEST(capi, validate)
{
    uint8_t wasm_prefix[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    EXPECT_TRUE(fizzy_validate(wasm_prefix, sizeof(wasm_prefix)));
    wasm_prefix[7] = 1;
    EXPECT_FALSE(fizzy_validate(wasm_prefix, sizeof(wasm_prefix)));
}

TEST(capi, parse)
{
    uint8_t wasm_prefix[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    auto module = fizzy_parse(wasm_prefix, sizeof(wasm_prefix));
    EXPECT_NE(module, nullptr);
    fizzy_free_module(module);
    wasm_prefix[7] = 1;
    EXPECT_EQ(fizzy_parse(wasm_prefix, sizeof(wasm_prefix)), nullptr);
}

TEST(capi, free_module_null)
{
    fizzy_free_module(nullptr);
}

TEST(capi, find_exported_function)
{
    /* wat2wasm
    (module
      (func $f (export "foo") (result i32) (i32.const 42))
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f030201000404017000000504010101020606017f0041000b07180403666f"
        "6f00000267310300037461620100036d656d02000a06010400412a0b");

    auto module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    uint32_t func_idx;
    EXPECT_TRUE(fizzy_find_exported_function(module, "foo", &func_idx));
    EXPECT_EQ(func_idx, 0);

    EXPECT_FALSE(fizzy_find_exported_function(module, "bar", &func_idx));
    EXPECT_FALSE(fizzy_find_exported_function(module, "g1", &func_idx));
    EXPECT_FALSE(fizzy_find_exported_function(module, "tab", &func_idx));
    EXPECT_FALSE(fizzy_find_exported_function(module, "mem", &func_idx));

    fizzy_free_module(module);
}

TEST(capi, instantiate)
{
    uint8_t wasm_prefix[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    auto module = fizzy_parse(wasm_prefix, sizeof(wasm_prefix));
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(module, nullptr, 0);
    EXPECT_NE(instance, nullptr);

    fizzy_free_instance(instance);
}

TEST(capi, instantiate_imported_function)
{
    /* wat2wasm
      (func (import "mod1" "foo1") (result i32))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020d01046d6f643104666f6f310000");
    auto module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    EXPECT_EQ(fizzy_instantiate(module, nullptr, 0), nullptr);

    module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    FizzyExternalFunction host_funcs[] = {
        {[](void*, FizzyInstance*, const FizzyValue*, size_t, int) {
             return FizzyExecutionResult{false, true, {42}};
         },
            nullptr}};

    auto instance = fizzy_instantiate(module, host_funcs, 1);
    EXPECT_NE(instance, nullptr);

    fizzy_free_instance(instance);
}

TEST(capi, free_instance_null)
{
    fizzy_free_instance(nullptr);
}

TEST(capi, memory_access_no_memory)
{
    /* wat2wasm
      (module)
    */
    const auto wasm = from_hex("0061736d01000000");
    auto module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(module, nullptr, 0);
    ASSERT_NE(instance, nullptr);

    EXPECT_EQ(fizzy_get_instance_memory_data(instance), nullptr);
    EXPECT_EQ(fizzy_get_instance_memory_size(instance), 0);

    fizzy_free_instance(instance);
}

TEST(capi, memory_access)
{
    /* wat2wasm
      (memory 1)
      (data (i32.const 1) "\11\22")
      (func (result i32)
        i32.const 0
        i32.load
      )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f0302010005030100010a0901070041002802000b0b08010041010b02112"
        "2");
    auto module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(module, nullptr, 0);
    ASSERT_NE(instance, nullptr);

    uint8_t* memory = fizzy_get_instance_memory_data(instance);
    ASSERT_NE(memory, nullptr);
    EXPECT_EQ(memory[0], 0);
    EXPECT_EQ(memory[1], 0x11);
    EXPECT_EQ(memory[2], 0x22);
    EXPECT_EQ(fizzy_get_instance_memory_size(instance), 65536);

    memory[0] = 0xaa;
    memory[1] = 0xbb;

    EXPECT_EQ(fizzy_execute(instance, 0, nullptr, 0).value.i64, 0x22bbaa);

    fizzy_free_instance(instance);
}

TEST(capi, imported_memory_access)
{
    /* wat2wasm
      (memory (import "mod" "mem") 1)
    */
    const auto wasm = from_hex("0061736d01000000020c01036d6f64036d656d020001");
    auto module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(module, nullptr, 0);
    EXPECT_EQ(instance, nullptr);
}

TEST(capi, execute)
{
    /* wat2wasm
      (func)
      (func (result i32) i32.const 42)
      (func (param i32 i32) (result i32)
        (i32.div_u (local.get 0) (local.get 1))
      )
      (func unreachable)
    */
    const auto wasm = from_hex(
        "0061736d01000000010e036000006000017f60027f7f017f030504000102000a150402000b0400412a0b070020"
        "0020016e0b0300000b");

    auto module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(module, nullptr, 0);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, 0), Result());
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, 0), Result(42));
    FizzyValue args[] = {{42}, {2}};
    EXPECT_THAT(fizzy_execute(instance, 2, args, 0), Result(21));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr, 0), Traps());

    fizzy_free_instance(instance);
}

TEST(capi, execute_with_host_function)
{
    /* wat2wasm
      (func (import "mod1" "foo1") (result i32))
      (func (import "mod1" "foo2") (param i32 i32) (result i32))
    */
    const auto wasm = from_hex(
        "0061736d01000000010b026000017f60027f7f017f021902046d6f643104666f6f310000046d6f643104666f6f"
        "320001");
    auto module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    FizzyExternalFunction host_funcs[] = {
        {[](void*, FizzyInstance*, const FizzyValue*, size_t, int) {
             return FizzyExecutionResult{false, true, {42}};
         },
            nullptr},
        {[](void*, FizzyInstance*, const FizzyValue* args, size_t, int) {
             return FizzyExecutionResult{false, true, {args[0].i64 / args[1].i64}};
         },
            nullptr}};

    auto instance = fizzy_instantiate(module, host_funcs, 2);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, 0), Result(42));

    FizzyValue args[] = {{42}, {2}};
    EXPECT_THAT(fizzy_execute(instance, 1, args, 0), Result(21));

    fizzy_free_instance(instance);
}

TEST(capi, imported_function_traps)
{
    /* wat2wasm
      (func (import "m" "foo") (result i32))
      (func (result i32)
        call 0
      )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020901016d03666f6f0000030201000a0601040010000b");
    auto module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    FizzyExternalFunction host_funcs[] = {
        {[](void*, FizzyInstance*, const FizzyValue*, size_t, int) {
             return FizzyExecutionResult{true, false, {}};
         },
            nullptr}};

    auto instance = fizzy_instantiate(module, host_funcs, 1);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, 0), Traps());

    fizzy_free_instance(instance);
}

TEST(capi, imported_function_void)
{
    /* wat2wasm
      (func (import "m" "foo"))
      (func
        call 0
      )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000020901016d03666f6f0000030201000a0601040010000b");
    auto module = fizzy_parse(wasm.data(), wasm.size());
    ASSERT_NE(module, nullptr);

    bool called = false;
    FizzyExternalFunction host_funcs[] = {
        {[](void* context, FizzyInstance*, const FizzyValue*, size_t, int) {
             *static_cast<bool*>(context) = true;
             return FizzyExecutionResult{false, false, {}};
         },
            &called}};

    auto instance = fizzy_instantiate(module, host_funcs, 1);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, 0), Result());
    EXPECT_TRUE(called);

    fizzy_free_instance(instance);
}

TEST(capi, imported_function_from_another_module)
{
    /* wat2wasm
    (module
      (func $sub (param $lhs i32) (param $rhs i32) (result i32)
        get_local $lhs
        get_local $rhs
        i32.sub)
      (export "sub" (func $sub))
    )
    */
    const auto bin1 = from_hex(
        "0061736d0100000001070160027f7f017f030201000707010373756200000a09010700200020016b0b");
    auto module1 = fizzy_parse(bin1.data(), bin1.size());
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(module1, nullptr, 0);
    ASSERT_NE(instance1, nullptr);

    /* wat2wasm
    (module
      (func $sub (import "m1" "sub") (param $lhs i32) (param $rhs i32) (result i32))

      (func $main (param i32) (param i32) (result i32)
        get_local 0
        get_local 1
        call $sub
      )
    )
    */
    const auto bin2 = from_hex(
        "0061736d0100000001070160027f7f017f020a01026d31037375620000030201000a0a0108002000200110000"
        "b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size());
    ASSERT_NE(module2, nullptr);

    // TODO fizzy_find_exported_function

    auto sub = [](void* context, FizzyInstance*, const FizzyValue* args, size_t,
                   int depth) -> FizzyExecutionResult {
        auto* called_instance = static_cast<FizzyInstance*>(context);
        return fizzy_execute(called_instance, 0, args, depth + 1);
    };
    FizzyExternalFunction host_funcs[] = {{sub, instance1}};

    auto instance2 = fizzy_instantiate(module2, host_funcs, 1);
    ASSERT_NE(instance2, nullptr);

    FizzyValue args[] = {{44}, {2}};
    EXPECT_THAT(fizzy_execute(instance2, 1, args, 0), Result(42));

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}
