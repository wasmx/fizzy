// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "module.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

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
