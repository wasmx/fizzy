// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "test/utils/typed_value.hpp"
#include <gtest/gtest.h>

using namespace fizzy;
using namespace fizzy::test;

TEST(typed_value, construct_contexpr)
{
    constexpr TypedValue i32{int32_t{-1}};
    static_assert(i32.type == ValType::i32);
    static_assert(i32.value.i32 == uint32_t(-1));
    EXPECT_EQ(i32.type, ValType::i32);
    EXPECT_EQ(i32.value.i64, uint32_t(-1));

    constexpr TypedValue u32{uint32_t{0xfffffffe}};
    static_assert(u32.type == ValType::i32);
    static_assert(u32.value.i32 == uint32_t{0xfffffffe});
    EXPECT_EQ(u32.type, ValType::i32);
    EXPECT_EQ(u32.value.i64, uint32_t{0xfffffffe});

    constexpr TypedValue i64{int64_t{-1}};
    static_assert(i64.type == ValType::i64);
    static_assert(i64.value.i64 == uint64_t(-1));
    EXPECT_EQ(i64.type, ValType::i64);
    EXPECT_EQ(i64.value.i64, uint64_t(-1));

    constexpr TypedValue u64{uint64_t{0xfffffffe}};
    static_assert(u64.type == ValType::i64);
    static_assert(u64.value.i64 == uint64_t{0xfffffffe});
    EXPECT_EQ(u64.type, ValType::i64);
    EXPECT_EQ(u64.value.i64, uint64_t{0xfffffffe});

    constexpr TypedValue f32{-1.17549435e-38f};
    static_assert(f32.type == ValType::f32);
    static_assert(f32.value.f32 == -1.17549435e-38f);
    EXPECT_EQ(f32.type, ValType::f32);
    EXPECT_EQ(f32.value.f32, -1.17549435e-38f);

    constexpr TypedValue f64{-2.2250738585072014e-308};
    static_assert(f64.type == ValType::f64);
    static_assert(f64.value.f64 == -2.2250738585072014e-308);
    EXPECT_EQ(f64.type, ValType::f64);
    EXPECT_EQ(f64.value.f64, -2.2250738585072014e-308);
}

TEST(typed_value, construct)
{
    const TypedValue i32{int32_t{-1}};
    EXPECT_EQ(i32.type, ValType::i32);
    EXPECT_EQ(i32.value.i32, uint32_t(-1));

    const TypedValue u32{uint32_t{0xfffffffe}};
    EXPECT_EQ(u32.type, ValType::i32);
    EXPECT_EQ(u32.value.i32, uint32_t{0xfffffffe});

    const TypedValue i64{int64_t{-1}};
    EXPECT_EQ(i64.type, ValType::i64);
    EXPECT_EQ(i64.value.i64, uint64_t(-1));

    const TypedValue u64{uint64_t{0xfffffffe}};
    EXPECT_EQ(u64.type, ValType::i64);
    EXPECT_EQ(u64.value.i64, uint64_t{0xfffffffe});

    const TypedValue f32{-1.17549435e-38f};
    EXPECT_EQ(f32.type, ValType::f32);
    EXPECT_EQ(f32.value.f32, -1.17549435e-38f);

    const TypedValue f64{-2.2250738585072014e-308};
    EXPECT_EQ(f64.type, ValType::f64);
    EXPECT_EQ(f64.value.f64, -2.2250738585072014e-308);
}

TEST(typed_value, u32_literal)
{
    static_assert(std::is_same_v<decltype(0_u32), uint32_t>);
    EXPECT_EQ(0_u32, uint32_t{0});
    EXPECT_EQ(1_u32, uint32_t{1});
    EXPECT_EQ(0xffffffff_u32, uint32_t{0xffffffff});

    EXPECT_THROW(operator""_u32(0x100000000), const char*);
}

TEST(typed_value, u64_literal)
{
    static_assert(std::is_same_v<decltype(0_u64), uint64_t>);
    EXPECT_EQ(0_u64, uint64_t{0});
    EXPECT_EQ(1_u64, uint64_t{1});
    EXPECT_EQ(0xffffffff_u64, uint64_t{0xffffffff});
    EXPECT_EQ(0x100000000_u64, uint64_t{0x100000000});
    EXPECT_EQ(0xffffffffffffffff_u64, uint64_t{0xffffffffffffffff});
}
