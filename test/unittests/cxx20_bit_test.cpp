// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bitcount_test_cases.hpp"
#include "cxx20/bit.hpp"
#include <gtest/gtest.h>
#include <array>

using namespace fizzy;
using namespace fizzy::test;
using namespace testing;

TEST(cxx20_bit, bit_cast_double_to_uint64)
{
    // A test case from https://en.cppreference.com/w/cpp/numeric/bit_cast#Example.
    EXPECT_EQ(bit_cast<uint64_t>(19880124.0), 0x4172f58bc0000000);
}

TEST(cxx20_bit, bit_cast_uint64_to_double)
{
    // A test case from https://en.cppreference.com/w/cpp/numeric/bit_cast#Example.
    EXPECT_EQ(bit_cast<double>(uint64_t{0x3fe9000000000000}), 0.781250);
}

TEST(cxx20_bit, bit_cast_uint32_to_int32)
{
    EXPECT_EQ(bit_cast<int32_t>(uint32_t{0x80000000}), -2147483648);
    EXPECT_EQ(bit_cast<int32_t>(uint32_t{0xffffffff}), -1);
}

TEST(cxx20_bit, bit_cast_int32_to_uint32)
{
    EXPECT_EQ(bit_cast<uint32_t>(int32_t{-2}), 0xfffffffe);
    EXPECT_EQ(bit_cast<uint32_t>(int32_t{1}), 1);
}

TEST(cxx20_bit, bit_cast_uint32_to_array)
{
    // Uses "byte-symmetric" value to avoid handling endianness.
    std::array<uint8_t, 4> bytes;
    bytes = bit_cast<decltype(bytes)>(uint32_t{0xaabbbbaa});
    EXPECT_EQ(bytes[0], 0xaa);
    EXPECT_EQ(bytes[1], 0xbb);
    EXPECT_EQ(bytes[2], 0xbb);
    EXPECT_EQ(bytes[3], 0xaa);
}

TEST(cxx20_bit, popcount32)
{
    for (const auto& [input, expected] : popcount32_test_cases)
        EXPECT_EQ(popcount(input), expected) << input;
}

TEST(cxx20_bit, popcount64)
{
    for (const auto& [input, expected] : popcount64_test_cases)
        EXPECT_EQ(popcount(input), expected) << input;
}

TEST(cxx20_bit, countl_zero32)
{
    EXPECT_EQ(countl_zero(uint32_t{0}), 32);
    EXPECT_EQ(countl_zero(uint32_t{0xffffffff}), 0);
    EXPECT_EQ(countl_zero(uint32_t{0x0000ffff}), 16);
    EXPECT_EQ(countl_zero(uint32_t{0xffff0000}), 0);
    EXPECT_EQ(countl_zero(uint32_t{0x00ffff00}), 8);
    EXPECT_EQ(countl_zero(uint32_t{0x00ff00ff}), 8);
}

TEST(cxx20_bit, countl_zero64)
{
    EXPECT_EQ(countl_zero(uint64_t{0}), 64);
    EXPECT_EQ(countl_zero(uint64_t{0xffffffffffffffff}), 0);
    EXPECT_EQ(countl_zero(uint64_t{0xffffffff00000000}), 0);
    EXPECT_EQ(countl_zero(uint64_t{0x00000000ffffffff}), 32);
    EXPECT_EQ(countl_zero(uint64_t{0x0000ffffffff0000}), 16);
    EXPECT_EQ(countl_zero(uint64_t{0x00ff00ff00ff00ff}), 8);
}

TEST(cxx20_bit, countr_zero32)
{
    EXPECT_EQ(countr_zero(uint32_t{0}), 32);
    EXPECT_EQ(countr_zero(uint32_t{0xffffffff}), 0);
    EXPECT_EQ(countr_zero(uint32_t{0x0000ffff}), 0);
    EXPECT_EQ(countr_zero(uint32_t{0xffff0000}), 16);
    EXPECT_EQ(countr_zero(uint32_t{0x00ffff00}), 8);
    EXPECT_EQ(countr_zero(uint32_t{0x00ff00ff}), 0);
}

TEST(cxx20_bit, countr_zero64)
{
    EXPECT_EQ(countr_zero(uint64_t{0}), 64);
    EXPECT_EQ(countr_zero(uint64_t{0xffffffffffffffff}), 0);
    EXPECT_EQ(countr_zero(uint64_t{0xffffffff00000000}), 32);
    EXPECT_EQ(countr_zero(uint64_t{0x00000000ffffffff}), 0);
    EXPECT_EQ(countr_zero(uint64_t{0x0000ffffffff0000}), 16);
    EXPECT_EQ(countr_zero(uint64_t{0x00ff00ff00ff00ff}), 0);
}
