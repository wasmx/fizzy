// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "cxx20/bit.hpp"
#include <gtest/gtest.h>
#include <array>

using namespace fizzy;
using namespace testing;


TEST(bit_cast, double_to_uint64)
{
    // A test case from https://en.cppreference.com/w/cpp/numeric/bit_cast#Example.
    EXPECT_EQ(bit_cast<uint64_t>(19880124.0), 0x4172f58bc0000000);
}

TEST(bit_cast, uint64_to_double)
{
    // A test case from https://en.cppreference.com/w/cpp/numeric/bit_cast#Example.
    EXPECT_EQ(bit_cast<double>(uint64_t{0x3fe9000000000000}), 0.781250);
}

TEST(bit_cast, uint32_to_int32)
{
    EXPECT_EQ(bit_cast<int32_t>(uint32_t{0x80000000}), -2147483648);
    EXPECT_EQ(bit_cast<int32_t>(uint32_t{0xffffffff}), -1);
}

TEST(bit_cast, int32_to_uint32)
{
    EXPECT_EQ(bit_cast<uint32_t>(int32_t{-2}), 0xfffffffe);
    EXPECT_EQ(bit_cast<uint32_t>(int32_t{1}), 1);
}

TEST(bit_cast, uint32_to_array)
{
    // Uses "byte-symmetric" value to avoid handling endianness.
    std::array<uint8_t, 4> bytes;
    bytes = bit_cast<decltype(bytes)>(uint32_t{0xaabbbbaa});
    EXPECT_EQ(bytes[0], 0xaa);
    EXPECT_EQ(bytes[1], 0xbb);
    EXPECT_EQ(bytes[2], 0xbb);
    EXPECT_EQ(bytes[3], 0xaa);
}
