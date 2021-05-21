// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "limits.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>
#include <test/utils/wasm_engine.hpp>

using namespace fizzy;
using namespace fizzy::test;

static const decltype(&create_fizzy_engine) all_engines[]{
    create_fizzy_engine, create_fizzy_c_engine, create_wabt_engine, create_wasm3_engine};

TEST(wasm_engine, validate_function_signature)
{
    EXPECT_NO_THROW(validate_function_signature(":"));
    EXPECT_NO_THROW(validate_function_signature("i:"));
    EXPECT_NO_THROW(validate_function_signature("iIiI:"));
    EXPECT_NO_THROW(validate_function_signature(":i"));
    EXPECT_NO_THROW(validate_function_signature(":iIiI"));
    EXPECT_NO_THROW(validate_function_signature("i:i"));
    EXPECT_NO_THROW(validate_function_signature("IiIi:IiIi"));
    EXPECT_THROW_MESSAGE(
        validate_function_signature(""), std::runtime_error, "Missing ':' delimiter");
    EXPECT_THROW_MESSAGE(
        validate_function_signature("i"), std::runtime_error, "Missing ':' delimiter");
    EXPECT_THROW_MESSAGE(validate_function_signature("::"), std::runtime_error,
        "Multiple occurrences of ':' found in signature");
    EXPECT_THROW_MESSAGE(validate_function_signature("i:i:i:"), std::runtime_error,
        "Multiple occurrences of ':' found in signature");
    EXPECT_THROW_MESSAGE(
        validate_function_signature("v:"), std::runtime_error, "Invalid type found in signature");
}

TEST(wasm_engine, parse_error)
{
    const auto wasm = "0102"_bytes;

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_FALSE(engine->parse(wasm));
        // Instantiate also performs parsing first.
        ASSERT_FALSE(engine->instantiate(wasm));
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

    // TODO: wasm3 doesn't care about imports, until execution
    // NOTE: wabt doesn't differentiate between parse/instantiate errors.
    for (auto engine_create_fn : {create_fizzy_engine, create_wabt_engine})
    {
        auto engine = engine_create_fn();
        EXPECT_FALSE(engine->instantiate(wasm));
    }
}

TEST(wasm_engine, find_function)
{
    /* wat2wasm
    (func $test (export "test") (param $a i32) (param $b i64) (param $c i32) (result i32)
      unreachable
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001080160037f7e7f017f03020100070801047465737400000a05010300000b");

    // NOTE: fizzy_c and wabt do not yet check signature
    for (auto engine_create_fn : {create_fizzy_engine, create_wasm3_engine})
    {
        auto engine = engine_create_fn();
        EXPECT_TRUE(engine->instantiate(wasm));
        EXPECT_TRUE(engine->find_function("test", "iIi:i").has_value());
        EXPECT_FALSE(engine->find_function("test", ":").has_value());
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

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate(wasm));
        const auto func = engine->find_function("test", ":");
        ASSERT_TRUE(func.has_value());
        const auto result = engine->execute(*func, {});
        ASSERT_TRUE(result.trapped);
        ASSERT_FALSE(result.value.has_value());
    }
}

TEST(wasm_engine, start_func)
{
    /* wat2wasm
    (global $g1 (mut i32) (i32.const 0))
    (func $start
      i32.const 13
      global.set $g1
    )
    (start 0)
    (func $test (export "test") (result i32)
      global.get $g1
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000108026000006000017f03030200010606017f0141000b070801047465737400010801000a"
        "0d020600410d24000b040023000b");

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate(wasm));
        ASSERT_FALSE(engine->find_function("notfound", "i:").has_value());
        const auto func = engine->find_function("test", ":i");
        ASSERT_TRUE(func.has_value());
        const auto result = engine->execute(*func, {});
        ASSERT_FALSE(result.trapped);
        ASSERT_TRUE(result.value.has_value());
        ASSERT_EQ(*result.value, 13);
    }
}

// This is another case of instantiate_error
TEST(wasm_engine, start_func_fail)
{
    /* wat2wasm
    (func $start
      unreachable
    )
    (start 0)
    (func $test (export "test") (result i32)
      i32.const 0
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000108026000006000017f0303020001070801047465737400010801000a0a020300000b0400"
        "41000b");

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_FALSE(engine->instantiate(wasm));
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

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate(wasm));
        ASSERT_FALSE(engine->find_function("notfound", "i:").has_value());
        const auto func = engine->find_function("test", "iii:i");
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

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate(wasm));
        ASSERT_FALSE(engine->find_function("notfound", "i:").has_value());
        const auto func = engine->find_function("test", "iIi:i");
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

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate(wasm));
        ASSERT_FALSE(engine->find_function("notfound", "i:").has_value());
        const auto func = engine->find_function("test", "iIi:I");
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

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate(wasm));
        const auto func = engine->find_function("test", ":");
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

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate(wasm));
        const auto func = engine->find_function("test", "ii:");
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

TEST(wasm_engine, host_function)
{
    /* wat2wasm
    (func $adler32 (import "env" "adler32") (param i32 i32) (result i32))
    (memory (export "memory") 1)
    (func $test (export "test") (param $a i32) (param $b i32) (result i32)
      local.get $a
      local.get $b
      call $adler32
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001070160027f7f017f020f0103656e760761646c6572333200000302010005030100010711"
        "02066d656d6f72790200047465737400010a0a0108002000200110000b");

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate(wasm));
        const auto func = engine->find_function("test", "ii:i");
        ASSERT_TRUE(func.has_value());

        const auto mem_init = bytes{0x12, 0, 0, 0x34};
        EXPECT_TRUE(engine->init_memory(mem_init));

        const auto result = engine->execute(*func, {0, 4});
        ASSERT_FALSE(result.trapped);
        ASSERT_TRUE(result.value.has_value());
        ASSERT_EQ(*result.value, 8388679);
    }
}

TEST(wasm_engine, start_with_host_function)
{
    /* wat2wasm
    (func $adler32 (import "env" "adler32") (param i32 i32) (result i32))
    (global $g1 (mut i32) (i32.const 0))
    (memory (export "memory") 1)
    (func $start
      i32.const 0
      i32.const 0x55aa55aa
      i32.store
      i32.const 0
      i32.const 32
      call $adler32
      global.set $g1
    )
    (start $start)
    (func $test (export "test") (result i32)
      global.get $g1
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010e0360027f7f017f6000006000017f020f0103656e760761646c65723332000003030201"
        "0205030100010606017f0141000b071102066d656d6f72790200047465737400020801010a1c021500410041aa"
        "aba9ad0536020041004120100024000b040023000b");

    for (auto engine_create_fn : all_engines)
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate(wasm));
        const auto func = engine->find_function("test", ":i");
        ASSERT_TRUE(func.has_value());

        const auto mem_check = engine->get_memory();
        const auto mem_expected = bytes{0xaa, 0x55, 0xaa, 0x55};
        EXPECT_EQ(mem_check.substr(0, 4), mem_expected);

        const auto result = engine->execute(*func, {});
        ASSERT_FALSE(result.trapped);
        ASSERT_TRUE(result.value.has_value());
        ASSERT_EQ(*result.value, 0x3d3801ff);
    }
}
