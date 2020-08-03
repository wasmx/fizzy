// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(execute_floating_point, f32_const)
{
    /* wat2wasm
    (func (result f32)
      f32.const 4194304.1
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017d030201000a09010700430000804a0b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Result(4194304.1f));
}

TEST(execute_floating_point, f64_const)
{
    /* wat2wasm
    (func (result f64)
      f64.const 8589934592.1
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017c030201000a0d010b0044cdcc0000000000420b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Result(8589934592.1));
}

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
    EXPECT_THAT(execute(*instance, 0, {1.001f, 6.006f}), Result(7.007f));
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
    EXPECT_THAT(execute(*instance, 0, {1.0011, 6.0066}), Result(7.0077));
}