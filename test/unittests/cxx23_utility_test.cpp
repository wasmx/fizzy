// Fizzy: A fast WebAssembly interpreter
// Copyright 2022 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "cxx23/utility.hpp"
#include "stack.hpp"
#include <gtest/gtest.h>
#include <array>
#include <vector>

using namespace fizzy;
using namespace testing;

TEST(cxx23_utility, to_underlying)
{
    enum class A
    {
        A = 0xff
    };
    static_assert(std::is_same_v<decltype(to_underlying(A::A)), int>);
    EXPECT_EQ(to_underlying(A::A), 0xff);

    enum class A8 : uint8_t
    {
        A = 0xff
    };
    static_assert(std::is_same_v<decltype(to_underlying(A8::A)), uint8_t>);
    EXPECT_EQ(to_underlying(A8::A), 0xff);

    enum class A32 : uint32_t
    {
        A = 0xff,
        B = 0xffffffff
    };
    static_assert(std::is_same_v<decltype(to_underlying(A32::A)), uint32_t>);
    EXPECT_EQ(to_underlying(A32::A), 0xff);
    EXPECT_EQ(to_underlying(A32::B), 0xffffffff);

    enum C
    {
        A = 0xff
    };
    EXPECT_EQ(to_underlying(C::A), 0xff);
}
