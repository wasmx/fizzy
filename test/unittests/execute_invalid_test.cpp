// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(execute_invalid, invalid_number_of_arguments)
{
    /* wat2wasm
    (func)
    (func (param i32))
    (func (param f32 f32))
    */
    const auto wasm = from_hex(
        "0061736d01000000010d0360000060017f0060027d7d000304030001020a0a0302000b02000b02000b");

    auto instance = instantiate(parse(wasm));

    EXPECT_THROW_MESSAGE(
        execute(*instance, 0, {1_u32}), std::invalid_argument, "too many arguments");

    EXPECT_THROW_MESSAGE(execute(*instance, 1, {}), std::invalid_argument, "too few arguments");
    EXPECT_THROW_MESSAGE(
        execute(*instance, 1, {1_u32, 2_u32}), std::invalid_argument, "too many arguments");

    EXPECT_THROW_MESSAGE(execute(*instance, 2, {}), std::invalid_argument, "too few arguments");
    EXPECT_THROW_MESSAGE(execute(*instance, 2, {0.0f}), std::invalid_argument, "too few arguments");
    EXPECT_THROW_MESSAGE(
        execute(*instance, 2, {0.0f, 0.0f, 0.0f}), std::invalid_argument, "too many arguments");
}

TEST(execute_invalid, wrong_argument_types)
{
    /* wat2wasm
    (func (param i32))
    (func (param f32 f32))
    */
    const auto wasm =
        from_hex("0061736d01000000010a0260017f0060027d7d0003030200010a070202000b02000b");

    auto instance = instantiate(parse(wasm));

    EXPECT_THROW_MESSAGE(
        execute(*instance, 0, {0_u64}), std::invalid_argument, "invalid type of the argument 0");
    EXPECT_THROW_MESSAGE(
        execute(*instance, 0, {0.0}), std::invalid_argument, "invalid type of the argument 0");
    EXPECT_THROW_MESSAGE(
        execute(*instance, 0, {0.0f}), std::invalid_argument, "invalid type of the argument 0");

    EXPECT_THROW_MESSAGE(
        execute(*instance, 1, {0, 0}), std::invalid_argument, "invalid type of the argument 0");
    EXPECT_THROW_MESSAGE(execute(*instance, 1, {0.0f, 0.0}), std::invalid_argument,
        "invalid type of the argument 1");
    EXPECT_THROW_MESSAGE(
        execute(*instance, 1, {0, 0.0f}), std::invalid_argument, "invalid type of the argument 0");
}
