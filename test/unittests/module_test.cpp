// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "module.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(module, functions)
{
    /* wat2wasm
      (func (import "m" "f1") (param i32 i32) (result i32))
      (func)
      (func (param i64) (local i32))
      (func (result f32) (f32.const 0))
    */
    const auto bin = from_hex(
        "0061736d0100000001120460027f7f017f60000060017e006000017d020801016d02663100000304030102030a"
        "110302000b0401017f0b070043000000000b");
    const auto module = parse(bin);

    ASSERT_EQ(module.get_function_count(), 4);
    EXPECT_EQ(
        module.get_function_type(0), (FuncType{{ValType::i32, ValType::i32}, {ValType::i32}}));
    EXPECT_EQ(module.get_function_type(1), (FuncType{}));
    EXPECT_EQ(module.get_function_type(2), (FuncType{{ValType::i64}, {}}));
    EXPECT_EQ(module.get_function_type(3), (FuncType{{}, {ValType::f32}}));

    EXPECT_EQ(module.get_code(1).instructions.size(), 1);
    EXPECT_EQ(module.get_code(1).local_count, 0);
    EXPECT_EQ(module.get_code(2).instructions.size(), 1);
    EXPECT_EQ(module.get_code(2).local_count, 1);
    EXPECT_EQ(module.get_code(3).instructions.size(), 2);
    EXPECT_EQ(module.get_code(3).local_count, 0);
}

TEST(module, globals)
{
    /* wat2wasm
      (global (import "m" "g1") (mut i32))
      (global (import "m" "g2") i64)
      (global f32 (f32.const 0))
      (global (mut f64) (f64.const 1))
    */
    const auto bin = from_hex(
        "0061736d01000000021102016d026731037f01016d026732037e000615027d0043000000000b7c014400000000"
        "0000f03f0b");
    const auto module = parse(bin);

    ASSERT_EQ(module.get_global_count(), 4);
    EXPECT_EQ(module.get_global_type(0).value_type, ValType::i32);
    EXPECT_TRUE(module.get_global_type(0).is_mutable);
    EXPECT_EQ(module.get_global_type(1).value_type, ValType::i64);
    EXPECT_FALSE(module.get_global_type(1).is_mutable);
    EXPECT_EQ(module.get_global_type(2).value_type, ValType::f32);
    EXPECT_FALSE(module.get_global_type(2).is_mutable);
    EXPECT_EQ(module.get_global_type(3).value_type, ValType::f64);
    EXPECT_TRUE(module.get_global_type(3).is_mutable);
}

TEST(module, table)
{
    /* wat2wasm
      (table 1 funcref)
    */
    const auto bin1 = from_hex("0061736d01000000040401700001");
    const auto module1 = parse(bin1);

    EXPECT_TRUE(module1.has_table());

    /* wat2wasm
      (table (import "m" "t") 1 funcref)
    */
    const auto bin2 = from_hex("0061736d01000000020901016d017401700001");
    const auto module2 = parse(bin2);

    EXPECT_TRUE(module2.has_table());

    /* wat2wasm
      (module)
    */
    const auto bin3 = from_hex("0061736d01000000");
    const auto module3 = parse(bin3);

    EXPECT_FALSE(module3.has_table());
}

TEST(module, memory)
{
    /* wat2wasm
      (memory 1)
    */
    const auto bin1 = from_hex("0061736d010000000503010001");
    const auto module1 = parse(bin1);

    EXPECT_TRUE(module1.has_memory());

    /* wat2wasm
      (memory (import "m" "m") 1)
    */
    const auto bin2 = from_hex("0061736d01000000020801016d016d020001");
    const auto module2 = parse(bin2);

    EXPECT_TRUE(module2.has_memory());

    /* wat2wasm
      (module)
    */
    const auto bin3 = from_hex("0061736d01000000");
    const auto module3 = parse(bin3);

    EXPECT_FALSE(module3.has_memory());
}
