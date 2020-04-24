// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "limits.hpp"
#include <gtest/gtest.h>
#include <test/utils/hex.hpp>
#include <test/utils/wasm_engine.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(wasm_engine, parse_error)
{
    const auto wasm = "0102"_bytes;

    for (auto engine_create_fn : {create_fizzy_engine, create_wabt_engine, create_wasm3_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_FALSE(engine->parse(wasm));
    }
}

TEST(wasm_engine, instantiate_error)
{
    /* wat2wasm
    (func $extfunc (import "env" "extfunc") (param i32) (result i32))
    (func $test (export "test")
      unreachable
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001090260017f017f600000020f0103656e760765787466756e630000030201010708010474"
        "65737400010a05010300000b");

    // TODO: parse/instantiate is not properly separated in wabt and wasm3
    // (and wasm3 doesn't care about imports, until execution)

    for (auto engine_create_fn : {create_fizzy_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_FALSE(engine->instantiate());
    }

    // NOTE: wabt doesn't differentiate between parse/instantiate errors
    for (auto engine_create_fn : {create_wabt_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_FALSE(engine->parse(wasm));
    }
}

TEST(wasm_engine, trapped)
{
    /* wat2wasm
    (func $test (export "test")
      unreachable
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001040160000003020100070801047465737400000a05010300000b");

    for (auto engine_create_fn : {create_fizzy_engine, create_wabt_engine, create_wasm3_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate());
        const auto func = engine->find_function("test");
        ASSERT_TRUE(func.has_value());
        const auto result = engine->execute(*func, {});
        ASSERT_TRUE(result.trapped);
        ASSERT_FALSE(result.value.has_value());
    }
}

TEST(wasm_engine, multi_i32_args_ret_i32)
{
    /* wat2wasm
    (func $test (export "test") (param $a i32) (param $b i32) (param $c i32) (result i32)
      local.get $a
      local.get $c
      i32.sub
      local.get $b
      i32.mul
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001080160037f7f7f017f03020100070801047465737400000a0c010a00200020026b20016c"
        "0b");

    for (auto engine_create_fn : {create_fizzy_engine, create_wabt_engine, create_wasm3_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate());
        ASSERT_FALSE(engine->find_function("notfound").has_value());
        const auto func = engine->find_function("test");
        ASSERT_TRUE(func.has_value());
        // (52 - 21) * 0x1fffffff => 0xdfffffe1
        const auto result = engine->execute(*func, {52, 0x1fffffff, 21});
        ASSERT_FALSE(result.trapped);
        ASSERT_TRUE(result.value.has_value());
        ASSERT_EQ(*result.value, 0xdfffffe1);
    }
}

TEST(wasm_engine, multi_mixed_args_ret_i32)
{
    /* wat2wasm
    (func $test (export "test") (param $a i32) (param $b i64) (param $c i32) (result i32)
      local.get $a
      local.get $c
      i32.sub
      i64.extend_i32_u
      local.get $b
      i64.mul
      i32.wrap_i64
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001080160037f7e7f017f03020100070801047465737400000a0e010c00200020026bad2001"
        "7ea70b");

    for (auto engine_create_fn : {create_fizzy_engine, create_wabt_engine, create_wasm3_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate());
        ASSERT_FALSE(engine->find_function("notfound").has_value());
        const auto func = engine->find_function("test");
        ASSERT_TRUE(func.has_value());
        // (52 - 21) * 0x1fffffff => 0xdfffffe1
        const auto result = engine->execute(*func, {52, 0x1fffffff, 21});
        ASSERT_FALSE(result.trapped);
        ASSERT_TRUE(result.value.has_value());
        ASSERT_EQ(*result.value, 0xdfffffe1);
    }
}

TEST(wasm_engine, multi_mixed_args_ret_i64)
{
    /* wat2wasm
    (func $test (export "test") (param $a i32) (param $b i64) (param $c i32) (result i64)
      local.get $a
      local.get $c
      i32.sub
      i64.extend_i32_u
      local.get $b
      i64.mul
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001080160037f7e7f017e03020100070801047465737400000a0d010b00200020026bad2001"
        "7e0b");

    for (auto engine_create_fn : {create_fizzy_engine, create_wabt_engine, create_wasm3_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate());
        ASSERT_FALSE(engine->find_function("notfound").has_value());
        const auto func = engine->find_function("test");
        ASSERT_TRUE(func.has_value());
        // (52 - 21) * 0x1fffffff => 0x3dfffffe1
        const auto result = engine->execute(*func, {52, 0x1fffffff, 21});
        ASSERT_FALSE(result.trapped);
        ASSERT_TRUE(result.value.has_value());
        ASSERT_EQ(*result.value, 0x3dfffffe1);
    }
}

TEST(wasm_engine, no_memory)
{
    /* wat2wasm
    (func $test (export "test"))
    */
    const auto wasm =
        from_hex("0061736d0100000001040160000003020100070801047465737400000a040102000b");

    for (auto engine_create_fn : {create_fizzy_engine, create_wabt_engine, create_wasm3_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate());
        const auto func = engine->find_function("test");
        EXPECT_TRUE(func.has_value());
        const auto mem = engine->get_memory();
        EXPECT_TRUE(mem.empty());
        EXPECT_FALSE(engine->init_memory({}));
    }
}

TEST(wasm_engine, memory)
{
    /* wat2wasm
    (memory (export "memory") 1)
    (func $test (export "test") (param $a i32) (param $b i32)
      local.get $b
      local.get $a
      i32.load
      i32.store
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027f7f00030201000503010001071102066d656d6f72790200047465737400000a"
        "0e010c00200120002802003602000b");

    for (auto engine_create_fn : {create_fizzy_engine, create_wabt_engine, create_wasm3_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate());
        const auto func = engine->find_function("test");
        ASSERT_TRUE(func.has_value());
        const auto mem_input = bytes{engine->get_memory()};
        ASSERT_FALSE(mem_input.empty());
        EXPECT_EQ(mem_input, bytes(64 * 1024, 0));

        const auto mem_init = bytes{0x12, 0, 0, 0x34};
        EXPECT_TRUE(engine->init_memory(mem_init));
        const auto mem_check = engine->get_memory();
        EXPECT_EQ(mem_check.substr(0, 4), mem_init);

        // Copy 32-bits from memory offset 0 to offset 4
        const auto result = engine->execute(*func, {0, 4});
        EXPECT_FALSE(result.trapped);
        EXPECT_FALSE(result.value.has_value());
        const auto mem_output = engine->get_memory();
        EXPECT_EQ(mem_output[4], 0x12);
        EXPECT_EQ(mem_output[5], 0);
        EXPECT_EQ(mem_output[6], 0);
        EXPECT_EQ(mem_output[7], 0x34);

        // Try initialising with oversized buffer.
        EXPECT_FALSE(engine->init_memory(bytes(PageSize + 4, 0)));
    }
}
