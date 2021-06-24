// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy::test;

TEST(capi_execute, execute)
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

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, nullptr), CResult());
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, nullptr), CResult(42_u32));
    FizzyValue args[] = {{42}, {2}};
    EXPECT_THAT(fizzy_execute(instance, 2, args, nullptr), CResult(21_u32));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr, nullptr), CTraps());

    fizzy_free_instance(instance);
}

TEST(capi_execute, execute_with_host_function)
{
    /* wat2wasm
      (func (import "mod1" "foo1") (result i32))
      (func (import "mod1" "foo2") (param i32 i32) (result i32))
    */
    const auto wasm = from_hex(
        "0061736d01000000010b026000017f60027f7f017f021902046d6f643104666f6f310000046d6f643104666f6f"
        "320001");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    const FizzyValueType inputs[] = {FizzyValueTypeI32, FizzyValueTypeI32};

    FizzyExternalFunction host_funcs[] = {
        {{FizzyValueTypeI32, nullptr, 0},
            [](void*, FizzyInstance*, const FizzyValue*, FizzyExecutionContext*) noexcept {
                return FizzyExecutionResult{false, true, {42}};
            },
            nullptr},
        {{FizzyValueTypeI32, &inputs[0], 2},
            [](void*, FizzyInstance*, const FizzyValue* args, FizzyExecutionContext*) noexcept {
                FizzyValue v;
                v.i32 = args[0].i32 / args[1].i32;
                return FizzyExecutionResult{false, true, {v}};
            },
            nullptr}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 2, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, nullptr), CResult(42_u32));

    FizzyValue args[] = {{42}, {2}};
    EXPECT_THAT(fizzy_execute(instance, 1, args, nullptr), CResult(21_u32));

    fizzy_free_instance(instance);
}

TEST(capi_execute, imported_function_traps)
{
    /* wat2wasm
      (func (import "m" "foo") (result i32))
      (func (result i32)
        call 0
      )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020901016d03666f6f0000030201000a0601040010000b");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyExternalFunction host_funcs[] = {{{FizzyValueTypeI32, nullptr, 0},
        [](void*, FizzyInstance*, const FizzyValue*, FizzyExecutionContext*) noexcept {
            return FizzyExecutionResult{true, false, {}};
        },
        nullptr}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, nullptr), CTraps());

    fizzy_free_instance(instance);
}

TEST(capi_execute, imported_function_void)
{
    /* wat2wasm
      (func (import "m" "foo"))
      (func
        call 0
      )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000020901016d03666f6f0000030201000a0601040010000b");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    bool called = false;
    FizzyExternalFunction host_funcs[] = {{{},
        [](void* context, FizzyInstance*, const FizzyValue*, FizzyExecutionContext*) noexcept {
            *static_cast<bool*>(context) = true;
            return FizzyExecutionResult{false, false, {}};
        },
        &called}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, nullptr), CResult());
    EXPECT_TRUE(called);

    fizzy_free_instance(instance);
}

TEST(capi_execute, imported_function_from_another_module)
{
    /* wat2wasm
    (module
      (func $sub (param $lhs i32) (param $rhs i32) (result i32)
        local.get $lhs
        local.get $rhs
        i32.sub)
      (export "sub" (func $sub))
    )
    */
    const auto bin1 = from_hex(
        "0061736d0100000001070160027f7f017f030201000707010373756200000a09010700200020016b0b");
    auto module1 = fizzy_parse(bin1.data(), bin1.size(), nullptr);
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance1, nullptr);

    FizzyExternalFunction func;
    ASSERT_TRUE(fizzy_find_exported_function(instance1, "sub", &func));

    /* wat2wasm
    (module
      (func $sub (import "m1" "sub") (param $lhs i32) (param $rhs i32) (result i32))

      (func $main (param i32) (param i32) (result i32)
        local.get 0
        local.get 1
        call $sub
      )
    )
    */
    const auto bin2 = from_hex(
        "0061736d0100000001070160027f7f017f020a01026d31037375620000030201000a0a0108002000200110000"
        "b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size(), nullptr);
    ASSERT_NE(module2, nullptr);

    auto instance2 = fizzy_instantiate(
        module2, &func, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance2, nullptr);

    FizzyValue args[] = {{44}, {2}};
    EXPECT_THAT(fizzy_execute(instance2, 1, args, nullptr), CResult(42_u32));

    fizzy_free_exported_function(&func);
    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi_execute, imported_function_from_another_module_via_host_function)
{
    /* wat2wasm
    (module
      (func $sub (param $lhs i32) (param $rhs i32) (result i32)
        local.get $lhs
        local.get $rhs
        i32.sub)
    )
    */
    const auto bin1 = from_hex("0061736d0100000001070160027f7f017f030201000a09010700200020016b0b");
    auto module1 = fizzy_parse(bin1.data(), bin1.size(), nullptr);
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance1, nullptr);

    /* wat2wasm
    (module
      (func $sub (import "m1" "sub") (param $lhs i32) (param $rhs i32) (result i32))

      (func $main (param i32) (param i32) (result i32)
        local.get 0
        local.get 1
        call $sub
      )
    )
    */
    const auto bin2 = from_hex(
        "0061736d0100000001070160027f7f017f020a01026d31037375620000030201000a0a0108002000200110000"
        "b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size(), nullptr);
    ASSERT_NE(module2, nullptr);

    auto sub = [](void* host_context, FizzyInstance*, const FizzyValue* args,
                   FizzyExecutionContext* ctx) noexcept -> FizzyExecutionResult {
        auto* instance = static_cast<FizzyInstance*>(host_context);
        return fizzy_execute(instance, 0, args, ctx);
    };

    const FizzyValueType inputs[] = {FizzyValueTypeI32, FizzyValueTypeI32};

    FizzyExternalFunction host_funcs[] = {{{FizzyValueTypeI32, &inputs[0], 2}, sub, instance1}};

    auto instance2 = fizzy_instantiate(module2, host_funcs, 1, nullptr, nullptr, nullptr, 0,
        FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance2, nullptr);

    FizzyValue args[] = {{44}, {2}};
    EXPECT_THAT(fizzy_execute(instance2, 1, args, nullptr), CResult(42_u32));

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi_execute, imported_table_from_another_module)
{
    /* wat2wasm
      (table (export "t") 10 30 funcref)
      (elem (i32.const 1) $f) ;; Table contents: uninit, f, uninit, ...
      (func $f (result i32) (i32.const 42))
    */
    const auto bin1 = from_hex(
        "0061736d010000000105016000017f0302010004050170010a1e070501017401000907010041010b01000a0601"
        "0400412a0b");
    auto module1 = fizzy_parse(bin1.data(), bin1.size(), nullptr);
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance1, nullptr);

    /* wat2wasm
      (type (func (result i32)))
      (table (import "m1" "t") 10 30 funcref)
      (func (result i32)
        (call_indirect (type 0) (i32.const 1))
      )
    */
    const auto bin2 = from_hex(
        "0061736d010000000105016000017f020b01026d3101740170010a1e030201000a0901070041011100000b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size(), nullptr);
    ASSERT_NE(module2, nullptr);

    FizzyExternalTable table;
    ASSERT_TRUE(fizzy_find_exported_table(instance1, "t", &table));

    auto instance2 = fizzy_instantiate(
        module2, nullptr, 0, &table, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance2, nullptr);

    EXPECT_THAT(fizzy_execute(instance2, 0, nullptr, nullptr), CResult(42_u32));

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi_execute, imported_memory_from_another_module)
{
    /* wat2wasm
      (memory (export "m") 1)
      (data (i32.const 10) "\aa\ff")
    */
    const auto bin1 = from_hex("0061736d010000000503010001070501016d02000b080100410a0b02aaff");
    auto module1 = fizzy_parse(bin1.data(), bin1.size(), nullptr);
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance1, nullptr);

    /* wat2wasm
      (memory (import "m1" "m") 1)
      (func (result i32)
        (i32.const 9)
        (i32.load)
      )
    */
    const auto bin2 = from_hex(
        "0061736d010000000105016000017f020901026d31016d020001030201000a0901070041092802000b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size(), nullptr);
    ASSERT_NE(module2, nullptr);

    FizzyExternalMemory memory;
    ASSERT_TRUE(fizzy_find_exported_memory(instance1, "m", &memory));

    auto instance2 = fizzy_instantiate(
        module2, nullptr, 0, nullptr, &memory, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance2, nullptr);

    EXPECT_THAT(fizzy_execute(instance2, 0, nullptr, nullptr), CResult(0x00ffaa00_u32));

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi_execute, imported_global_from_another_module)
{
    /* wat2wasm
      (global (export "g") i32 (i32.const 42))
    */
    const auto bin1 = from_hex("0061736d010000000606017f00412a0b07050101670300");
    auto module1 = fizzy_parse(bin1.data(), bin1.size(), nullptr);
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance1, nullptr);

    /* wat2wasm
    (module
      (global (import "m1" "g") i32)
      (func (result i32)
        global.get 0
      )
    )
    */
    const auto bin2 =
        from_hex("0061736d010000000105016000017f020901026d310167037f00030201000a0601040023000b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size(), nullptr);
    ASSERT_NE(module2, nullptr);

    FizzyExternalGlobal global;
    ASSERT_TRUE(fizzy_find_exported_global(instance1, "g", &global));

    auto instance2 = fizzy_instantiate(
        module2, nullptr, 0, nullptr, nullptr, &global, 1, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance2, nullptr);

    EXPECT_THAT(fizzy_execute(instance2, 0, nullptr, nullptr), CResult(42_u32));

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi_execute, execute_with_execution_conext)
{
    /* wat2wasm
      (func (result i32) i32.const 42)
      (func (result i32) call 0)
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f03030200000a0b020400412a0b040010000b");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    auto* ctx = fizzy_create_execution_context(0);
    auto* depth = fizzy_get_execution_context_depth(ctx);

    EXPECT_EQ(*depth, 0);
    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, ctx), CResult(42_u32));
    EXPECT_EQ(*depth, 0);
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, ctx), CResult(42_u32));
    EXPECT_EQ(*depth, 0);

    *depth = 2047;
    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, ctx), CResult(42_u32));
    EXPECT_EQ(*depth, 2047);
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, ctx), CTraps());
    EXPECT_EQ(*depth, 2047);

    *depth = 2048;
    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, ctx), CTraps());
    EXPECT_EQ(*depth, 2048);
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, ctx), CTraps());
    EXPECT_EQ(*depth, 2048);

    fizzy_free_execution_context(ctx);
    fizzy_free_instance(instance);
}

TEST(capi_execute, execute_with_metered_execution_conext)
{
    /* wat2wasm
      (func (result i32) i32.const 42)
      (func (result i32) call 0)
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f03030200000a0b020400412a0b040010000b");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    auto* ctx = fizzy_create_metered_execution_context(0, 100);
    auto* ticks = fizzy_get_execution_context_ticks(ctx);
    EXPECT_EQ(*ticks, 100);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, ctx), CResult(42_u32));
    EXPECT_EQ(*ticks, 98);
    *ticks = 100;
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, ctx), CResult(42_u32));
    EXPECT_EQ(*ticks, 96);

    *ticks = 4;
    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, ctx), CResult(42_u32));
    EXPECT_EQ(*ticks, 2);
    *ticks = 4;
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, ctx), CResult(42_u32));
    EXPECT_EQ(*ticks, 0);

    *ticks = 2;
    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, ctx), CResult(42_u32));
    EXPECT_EQ(*ticks, 0);
    *ticks = 2;
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, ctx), CTraps());

    *ticks = 1;
    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, ctx), CTraps());
    *ticks = 1;
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, ctx), CTraps());

    *ticks = 0;
    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, ctx), CTraps());
    *ticks = 0;
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, ctx), CTraps());

    fizzy_free_execution_context(ctx);
    fizzy_free_instance(instance);
}
