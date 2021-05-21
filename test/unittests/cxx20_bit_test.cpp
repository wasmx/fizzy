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
    for (const auto& [input, expected_popcount, expected_countl_zero, expected_countr_zero] :
        bitcount32_test_cases)
        EXPECT_EQ(popcount(input), expected_popcount) << input;
}

TEST(cxx20_bit, popcount64)
{
    for (const auto& [input, expected_popcount, expected_countl_zero, expected_countr_zero] :
        bitcount64_test_cases)
        EXPECT_EQ(popcount(input), expected_popcount) << input;
}

TEST(cxx20_bit, countl_zero32)
{
    for (const auto& [input, expected_popcount, expected_countl_zero, expected_countr_zero] :
        bitcount32_test_cases)
        EXPECT_EQ(countl_zero(input), expected_countl_zero) << input;
}

TEST(cxx20_bit, countl_zero64)
{
    for (const auto& [input, expected_popcount, expected_countl_zero, expected_countr_zero] :
        bitcount64_test_cases)
        EXPECT_EQ(countl_zero(input), expected_countl_zero) << input;
}

TEST(cxx20_bit, countr_zero32)
{
    for (const auto& [input, expected_popcount, expected_countl_zero, expected_countr_zero] :
        bitcount32_test_cases)
        EXPECT_EQ(countr_zero(input), expected_countr_zero) << input;
}

TEST(cxx20_bit, countr_zero64)
{
    for (const auto& [input, expected_popcount, expected_countl_zero, expected_countr_zero] :
        bitcount64_test_cases)
        EXPECT_EQ(countr_zero(input), expected_countr_zero) << input;
}
