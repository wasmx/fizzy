// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "value.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

TEST(value, value_initialization)
{
    Value v{};
    EXPECT_EQ(v.i64, 0);
}

TEST(value, constructor_from_i64)
{
    Value v = 1;
    EXPECT_EQ(v.i64, 1);
    v = 2;
    EXPECT_EQ(v.i64, 2);
    v = false;
    EXPECT_EQ(v.i64, 0);
    v = true;
    EXPECT_EQ(v.i64, 1);
    v = uint64_t{111};
    EXPECT_EQ(v.i64, 111);

    constexpr auto max_i64 = std::numeric_limits<uint64_t>::max();
    v = max_i64;
    EXPECT_EQ(v.i64, max_i64);
}

TEST(value, implicit_conversion_to_i64)
{
    const Value v{1};
    const uint64_t x = v;
    EXPECT_EQ(x, 1);
}
