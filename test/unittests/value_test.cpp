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
    v = uint64_t{111};
    EXPECT_EQ(v.i64, 111);

    constexpr auto max_i64 = std::numeric_limits<uint64_t>::max();
    v = max_i64;
    EXPECT_EQ(v.i64, max_i64);
}

TEST(value, constructor_from_unsigned_ints)
{
    EXPECT_EQ(Value{uint32_t{0xdededefe}}.i64, 0xdededefe);
    EXPECT_EQ(Value{uint64_t{0xdededededededefe}}.i64, 0xdededededededefe);
    EXPECT_EQ(Value{static_cast<unsigned long>(0xdededededededefe)}.i64, 0xdededededededefe);
    EXPECT_EQ(Value{static_cast<unsigned long long>(0xdededededededefe)}.i64, 0xdededededededefe);
}

TEST(value, constructor_from_signed_ints)
{
    EXPECT_EQ(Value{int32_t{-3}}.i64, 0xfffffffd);
    EXPECT_EQ(Value{int64_t{-3}}.i64, 0xfffffffffffffffd);
}

TEST(value, as_integer)
{
    const Value v{0xfffffffffffffffe};
    EXPECT_EQ(v.as<uint64_t>(), 0xfffffffffffffffe);
    EXPECT_EQ(v.as<uint32_t>(), 0xfffffffe);
    EXPECT_EQ(v.as<int64_t>(), -2);
    EXPECT_EQ(v.as<int32_t>(), -2);
}

TEST(value, implicit_conversion_to_i64)
{
    const Value v{1};
    const uint64_t x = v;
    EXPECT_EQ(x, 1);
}
