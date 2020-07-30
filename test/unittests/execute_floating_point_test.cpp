// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(execute_floating_point, f32_add)
{
    /* wat2wasm
    (func (param f32 f32) (result f32)
      local.get 0
      local.get 1
      f32.add
    )
    */
    const auto wasm = from_hex("0061736d0100000001070160027d7d017d030201000a0901070020002001920b");

    auto instance = instantiate(parse(wasm));
    const auto result = execute(*instance, 0, {1.001f, 6.006f});
    EXPECT_EQ(result.value.f32, 7.007f);
}

TEST(execute_floating_point, f64_add)
{
    /* wat2wasm
    (func (param f64 f64) (result f64)
      local.get 0
      local.get 1
      f64.add
    )
    */
    const auto wasm = from_hex("0061736d0100000001070160027c7c017c030201000a0901070020002001a00b");

    auto instance = instantiate(parse(wasm));
    const auto result = execute(*instance, 0, {1.0011, 6.0066});
    EXPECT_EQ(result.value.f64, 7.0077);
}
