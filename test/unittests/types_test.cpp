// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "types.hpp"
#include <gtest/gtest.h>

using namespace fizzy;
using namespace testing;

TEST(types, functype)
{
    const FuncType functype_v = {};
    const FuncType functype_v_v = {};
    const FuncType functype_i = {{ValType::i32}, {}};
    const FuncType functype_ii = {{ValType::i32, ValType::i32}, {}};
    const FuncType functype_I = {{ValType::i64}, {}};
    const FuncType functype_i_i = {{ValType::i32}, {ValType::i32}};
    const FuncType functype_i_ii = {{ValType::i32}, {ValType::i32, ValType::i32}};
    const FuncType functype_ii_i = {{ValType::i32, ValType::i32}, {ValType::i32}};

    EXPECT_TRUE(functype_v == functype_v);
    EXPECT_TRUE(functype_v == functype_v_v);
    EXPECT_FALSE(functype_v != functype_v);
    EXPECT_FALSE(functype_v != functype_v_v);

    EXPECT_FALSE(functype_v == functype_i);
    EXPECT_FALSE(functype_i == functype_ii);
    EXPECT_FALSE(functype_i == functype_i_i);
    EXPECT_FALSE(functype_i == functype_i_ii);
    EXPECT_FALSE(functype_i == functype_ii_i);
    EXPECT_FALSE(functype_i == functype_I);
    EXPECT_FALSE(functype_I == functype_i_i);
    EXPECT_FALSE(functype_I == functype_i_ii);
    EXPECT_FALSE(functype_I == functype_ii_i);

    EXPECT_TRUE(functype_v != functype_i);
    EXPECT_TRUE(functype_i != functype_ii);
    EXPECT_TRUE(functype_i != functype_i_i);
    EXPECT_TRUE(functype_i != functype_i_ii);
    EXPECT_TRUE(functype_i != functype_ii_i);
    EXPECT_TRUE(functype_i != functype_I);
    EXPECT_TRUE(functype_I != functype_i_i);
    EXPECT_TRUE(functype_I != functype_i_ii);
    EXPECT_TRUE(functype_I != functype_ii_i);
}
