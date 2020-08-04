// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(test_utils, hex)
{
    EXPECT_EQ(hex(0x01), "01");
    bytes data = "0102"_bytes;
    EXPECT_EQ(hex(data), "0102");
    EXPECT_EQ(hex(data.data(), data.size()), "0102");
}

TEST(test_utils, from_hex)
{
    EXPECT_EQ(from_hex(""), bytes{});
    // Since the _bytes operator uses from_hex internally, lets check raw bytes first.
    EXPECT_EQ(from_hex("00112233445566778899aabbccddeeffAABBCCDDEEFF"),
        bytes({0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd,
            0xee, 0xff, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}));
    EXPECT_EQ(from_hex("00112233445566778899aabbccddeeffAABBCCDDEEFF"),
        "00112233445566778899aabbccddeeffAABBCCDDEEFF"_bytes);
    EXPECT_THROW_MESSAGE(from_hex("a"), std::length_error, "the length of the input is odd");
    EXPECT_THROW_MESSAGE(from_hex("aaa"), std::length_error, "the length of the input is odd");
    EXPECT_THROW_MESSAGE(from_hex("gg"), std::out_of_range, "not a hex digit");
    EXPECT_THROW_MESSAGE(from_hex("GG"), std::out_of_range, "not a hex digit");
    EXPECT_THROW_MESSAGE(from_hex("fg"), std::out_of_range, "not a hex digit");
    EXPECT_THROW_MESSAGE(from_hex("FG"), std::out_of_range, "not a hex digit");
}

TEST(test_utils, result_signed_int)
{
    EXPECT_THAT(ExecutionResult{Value{-1}}, Result(-1));
    constexpr auto v = std::numeric_limits<int32_t>::min();
    EXPECT_THAT(ExecutionResult{Value{v}}, Result(v));
}
