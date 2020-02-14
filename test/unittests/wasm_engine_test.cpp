#include <gtest/gtest.h>
#include <test/utils/hex.hpp>
#include <test/utils/wasm_engine.hpp>

using namespace fizzy;
using namespace fizzy::test;

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

    // TODO: wabt_engine can't support this due to typing shortcomings
    for (auto engine_create_fn : {create_fizzy_engine, create_wasm3_engine})
    {
        auto engine = engine_create_fn();
        ASSERT_TRUE(engine->parse(wasm));
        ASSERT_TRUE(engine->instantiate());
        const auto func = engine->find_function("test");
        ASSERT_TRUE(func.has_value());
        // (52 - 21) * 0x1fffffff => 0xdfffffe1
        const auto result = engine->execute(*func, {52, 0x1fffffff, 21});
        ASSERT_FALSE(result.trapped);
        ASSERT_TRUE(result.value.has_value());
        ASSERT_EQ(*result.value, 0xdfffffe1);
    }
}
