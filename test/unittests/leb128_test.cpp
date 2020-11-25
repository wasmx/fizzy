// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "leb128.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

namespace
{
/// A leb128u_decode() wrapper for convenient testing.
template <typename T>
inline auto leb128u_decode(bytes_view input)
{
    return fizzy::leb128u_decode<T>(input.begin(), input.end());
}

/// A leb128s_decode() wrapper for convenient testing.
template <typename T>
inline auto leb128s_decode(bytes_view input)
{
    return fizzy::leb128s_decode<T>(input.begin(), input.end());
}
}  // namespace

TEST(leb128, decode_u64)
{
    constexpr std::pair<std::string_view, uint64_t> test_cases[]{
        {"00", 0},                                     // 0
        {"808000", 0},                                 // 0 with leading zeroes
        {"01", 1},                                     // 1
        {"81808000", 1},                               // 1 with leading zeroes
        {"81808080808080808000", 1},                   // 1 with max leading zeroes
        {"e58e26", 624485},                            // 624485
        {"e58ea6808000", 624485},                      // 624485 with leading zeroes
        {"ffffffff07", 0x7fffffff},                    //
        {"8080808008", 0x80000000},                    //
        {"ffffffffffffff00", 562949953421311},         // bigger than int32
        {"ffffffffffffff808000", 562949953421311},     // bigger than int32 with zeroes
        {"ffffffffffffffff7f", 0x7fffffffffffffff},    //
        {"80808080808080808001", 0x8000000000000000},  //
        {"ffffffffffffffffff01", std::numeric_limits<uint64_t>::max()},  // max
    };

    for (const auto& [input_hex, expected] : test_cases)
    {
        const auto input = from_hex(input_hex);
        const auto [value, ptr] = leb128u_decode<uint64_t>(input);
        EXPECT_EQ(value, expected) << input_hex;
        EXPECT_EQ(ptr, &input[0] + input.size()) << input_hex;
    }
}

TEST(leb128, decode_u64_invalid)
{
    bytes encoded_624485_invalid{0xe5, 0x8e, 0xa6};
    EXPECT_THROW_MESSAGE(
        leb128u_decode<uint64_t>(encoded_624485_invalid), parser_error, "unexpected EOF");

    bytes encoded_1_too_many_leading_zeroes{
        0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(encoded_1_too_many_leading_zeroes), parser_error,
        "invalid LEB128 encoding: too many bytes");

    bytes encoded_max_leading_zeroes{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x81, 0x00};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(encoded_max_leading_zeroes), parser_error,
        "invalid LEB128 encoding: too many bytes");

    bytes encoded_max_unused_bits_set{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(encoded_max_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits set");

    bytes encoded_max_some_unused_bits_set{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x19};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(encoded_max_some_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits set");
}

TEST(leb128, decode_u32)
{
    constexpr std::pair<std::string_view, uint32_t> test_cases[]{
        {"00", 0},                                             // 0
        {"808000", 0},                                         // 0 with leading zeroes
        {"01", 1},                                             // 1
        {"81808000", 1},                                       // 1 with leading zeroes
        {"8180808000", 1},                                     // 1 with max leading zeroes
        {"8200", 2},                                           // 2 with leading zeroes
        {"e58e26", 624485},                                    // 624485
        {"e58ea68000", 624485},                                // 624485 with leading zeroes
        {"ffffffff07", 0x7fffffff},                            //
        {"8080808008", 0x80000000},                            //
        {"ffffffff0f", std::numeric_limits<uint32_t>::max()},  // max
    };

    for (const auto& [input_hex, expected] : test_cases)
    {
        const auto input = from_hex(input_hex);
        const auto [value, ptr] = leb128u_decode<uint32_t>(input);
        EXPECT_EQ(value, expected) << input_hex;
        EXPECT_EQ(ptr, &input[0] + input.size()) << input_hex;
    }
}

TEST(leb128, decode_u32_invalid)
{
    bytes encoded_624485_invalid{0xe5, 0x8e, 0xa6};
    EXPECT_THROW_MESSAGE(
        leb128u_decode<uint32_t>(encoded_624485_invalid), parser_error, "unexpected EOF");

    bytes encoded_1_too_many_leading_zeroes{0x81, 0x80, 0x80, 0x80, 0x80, 0x00};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_1_too_many_leading_zeroes), parser_error,
        "invalid LEB128 encoding: too many bytes");

    bytes encoded_max_leading_zeroes{0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_max_leading_zeroes), parser_error,
        "invalid LEB128 encoding: too many bytes");

    bytes encoded_max_unused_bits_set{0xff, 0xff, 0xff, 0xff, 0x7f};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_max_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits set");

    bytes encoded_2_unused_bits_set{0x82, 0x80, 0x80, 0x80, 0x70};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_2_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits set");

    bytes encoded_0_some_unused_bits_set{0x80, 0x80, 0x80, 0x80, 0x1f};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_0_some_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits set");
}

TEST(leb128, decode_u8)
{
    constexpr std::pair<std::string_view, uint64_t> test_cases[]{
        {"00", 0},      // 0
        {"8000", 0},    // 0 with leading zero
        {"01", 1},      // 1
        {"8100", 1},    // 1 with leading zero
        {"e501", 229},  // 229
        {"ff01", 255},  // max
    };

    for (const auto& [input_hex, expected] : test_cases)
    {
        const auto input = from_hex(input_hex);
        const auto [value, ptr] = leb128u_decode<uint8_t>(input);
        EXPECT_EQ(value, expected) << input_hex;
        EXPECT_EQ(ptr, &input[0] + input.size()) << input_hex;
    }
}

TEST(leb128, decode_u8_invalid)
{
    bytes encoded_1_too_many_leading_zeroes{0x81, 0x80, 0x80};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(encoded_1_too_many_leading_zeroes.data()),
        parser_error, "invalid LEB128 encoding: too many bytes");

    bytes encoded_too_big{0xe5, 0x8e, 0x26};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(encoded_too_big.data()), parser_error,
        "invalid LEB128 encoding: too many bytes");

    bytes encoded_max_unused_bits_set{0xff, 0x7f};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(encoded_max_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits set");

    bytes encoded_max_some_unused_bits_set{0xff, 0x19};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(encoded_max_some_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits set");
}

TEST(leb128, decode_out_of_buffer)
{
    constexpr auto m = "unexpected EOF";
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(bytes{}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(bytes{0x80}), parser_error, m);

    EXPECT_THROW_MESSAGE(leb128u_decode<uint16_t>(bytes{}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint16_t>(bytes{0x81}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint16_t>(bytes{0x82, 0x81}), parser_error, m);

    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(bytes{}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(bytes{0x91}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(bytes{0x91, 0x92}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(bytes{0x91, 0x92, 0x93}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(bytes{0x91, 0x92, 0x93, 0x94}), parser_error, m);

    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(bytes{}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(bytes{0xa1}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(bytes{0xa1, 0xa2}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(bytes{0xa1, 0xa2, 0xa3}), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(bytes{0xa1, 0xa2, 0xa3, 0xa4}), parser_error, m);
    EXPECT_THROW_MESSAGE(
        leb128u_decode<uint64_t>(bytes{0xa1, 0xa2, 0xa3, 0xa4, 0xa5}), parser_error, m);
    EXPECT_THROW_MESSAGE(
        leb128u_decode<uint64_t>(bytes{0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6}), parser_error, m);
    EXPECT_THROW_MESSAGE(
        leb128u_decode<uint64_t>(bytes{0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7}), parser_error, m);
    EXPECT_THROW_MESSAGE(
        leb128u_decode<uint64_t>(bytes{0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8}),
        parser_error, m);
    EXPECT_THROW_MESSAGE(
        leb128u_decode<uint64_t>(bytes{0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa7, 0xa8, 0xa9}),
        parser_error, m);
}

TEST(leb128, decode_s64)
{
    constexpr std::pair<std::string_view, int64_t> test_cases[]{
        {"00", 0},                                  // 0
        {"808000", 0},                              // 0 with leading zeroes
        {"01", 1},                                  //
        {"81808000", 1},                            // 1 with leading zeroes
        {"81808080808080808000", 1},                // 1 with max leading zeroes
        {"7f", -1},                                 //
        {"ffffffffffffffffff7f", -1},               // -1 with leading 1s
        {"7e", -2},                                 //
        {"fe7f", -2},                               // -2 with leading 1s
        {"feff7f", -2},                             // -2 with leading 1s
        {"e58e26", 624485},                         //
        {"e58ea6808000", 624485},                   // 624485 with leading zeroes
        {"c0bb78", -123456},                        //
        {"9bf159", -624485},                        //
        {"ffffffffffffff00", 562949953421311},      // bigger than int32
        {"ffffffffffffff808000", 562949953421311},  // bigger than int32 with zeroes
    };

    for (const auto& [input_hex, expected] : test_cases)
    {
        const auto input = from_hex(input_hex);
        const auto [value, ptr] = leb128s_decode<int64_t>(input);
        EXPECT_EQ(value, expected) << input_hex;
        EXPECT_EQ(ptr, &input[0] + input.size()) << input_hex;
    }
}

TEST(leb128, decode_s64_invalid)
{
    const bytes encoded_1_too_many_leading_zeroes{
        0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW_MESSAGE(leb128s_decode<int64_t>(encoded_1_too_many_leading_zeroes), parser_error,
        "invalid LEB128 encoding: too many bytes");

    const bytes encoded_minus1_too_many_leading_1s{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    EXPECT_THROW_MESSAGE(leb128s_decode<int64_t>(encoded_minus1_too_many_leading_1s), parser_error,
        "invalid LEB128 encoding: too many bytes");

    const bytes minus1_unused_bits_unset{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    EXPECT_THROW_MESSAGE(leb128s_decode<int64_t>(minus1_unused_bits_unset), parser_error,
        "invalid LEB128 encoding: unused bits not equal to sign bit");

    const bytes minus1_some_unused_bits_unset{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x79};
    EXPECT_THROW_MESSAGE(leb128s_decode<int64_t>(minus1_some_unused_bits_unset), parser_error,
        "invalid LEB128 encoding: unused bits not equal to sign bit");
}

TEST(leb128, decode_s32)
{
    constexpr std::pair<std::string_view, int32_t> test_cases[]{
        {"00", 0},               // 0
        {"808000", 0},           // 0 with leading zeroes
        {"01", 1},               //
        {"81808000", 1},         // 1 with leading zeroes
        {"8180808000", 1},       // 1 with max leading zeroes
        {"7f", -1},              //
        {"ffffffff7f", -1},      // -1 with leading 1s
        {"7e", -2},              //
        {"fe7f", -2},            // -2 with leading 1s
        {"feff7f", -2},          // -2 with leading 1s
        {"e58e26", 624485},      //
        {"e58ea68000", 624485},  // 624485 with leading zeroes
        {"c0bb78", -123456},
        {"9bf159", -624485},
        {"8180808078", -2147483647},
        {"8080808078", std::numeric_limits<int32_t>::min()},
    };

    for (const auto& [input_hex, expected] : test_cases)
    {
        const auto input = from_hex(input_hex);
        const auto [value, ptr] = leb128s_decode<int32_t>(input);
        EXPECT_EQ(value, expected) << input_hex;
        EXPECT_EQ(ptr, &input[0] + input.size()) << input_hex;
    }
}

TEST(leb128, decode_s32_invalid)
{
    const bytes encoded_0_unused_bits_set{0x80, 0x80, 0x80, 0x80, 0x70};
    EXPECT_THROW_MESSAGE(leb128s_decode<int32_t>(encoded_0_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits not equal to sign bit");

    const bytes encoded_0_some_unused_bits_set{0x80, 0x80, 0x80, 0x80, 0x10};
    EXPECT_THROW_MESSAGE(leb128s_decode<int32_t>(encoded_0_some_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits not equal to sign bit");

    const bytes minus1_unused_bits_unset{0xff, 0xff, 0xff, 0xff, 0x0f};
    EXPECT_THROW_MESSAGE(leb128s_decode<int32_t>(minus1_unused_bits_unset), parser_error,
        "invalid LEB128 encoding: unused bits not equal to sign bit");

    const bytes minus1_some_unused_bits_set{0xff, 0xff, 0xff, 0xff, 0x4f};
    EXPECT_THROW_MESSAGE(leb128s_decode<int32_t>(minus1_some_unused_bits_set), parser_error,
        "invalid LEB128 encoding: unused bits not equal to sign bit");
}

TEST(leb128, decode_s8)
{
    constexpr std::pair<std::string_view, int64_t> test_cases[]{
        {"00", 0},    // 0
        {"8000", 0},  // 0 with leading zero
        {"01", 1},    // 1
        {"8100", 1},  // 1 with leading zero
        {"ff7f", -1},
        {"fe7f", -2},
        {"40", -64},
        {"817f", -127},
        {"807f", std::numeric_limits<int8_t>::min()},
    };

    for (const auto& [input_hex, expected] : test_cases)
    {
        const auto input = from_hex(input_hex);
        const auto [value, ptr] = leb128s_decode<int8_t>(input);
        EXPECT_EQ(value, expected) << input_hex;
        EXPECT_EQ(ptr, &input[0] + input.size()) << input_hex;
    }
}

TEST(leb128, decode_s8_invalid)
{
    const bytes encoded_1_too_many_leading_zeroes{0x81, 0x80, 0x80};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(encoded_1_too_many_leading_zeroes), parser_error,
        "invalid LEB128 encoding: too many bytes");

    const bytes encoded_too_big{0xe5, 0x8e, 0x26};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(encoded_too_big), parser_error,
        "invalid LEB128 encoding: too many bytes");

    const bytes minus1_unused_bits_unset{0xff, 0x01};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(minus1_unused_bits_unset), parser_error,
        "invalid LEB128 encoding: unused bits not equal to sign bit");

    const bytes minus2_unused_bits_unset{0xfe, 0x01};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(minus2_unused_bits_unset), parser_error,
        "invalid LEB128 encoding: unused bits not equal to sign bit");

    const bytes minus1_some_unused_bits_unset{0xff, 0x71};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(minus1_some_unused_bits_unset), parser_error,
        "invalid LEB128 encoding: unused bits not equal to sign bit");
}

TEST(leb128, decode_s_out_of_buffer)
{
    constexpr auto m = "unexpected EOF";
    const auto input = bytes{0x82, 0x81};

    EXPECT_THROW_MESSAGE(leb128s_decode<int16_t>(nullptr, nullptr), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128s_decode<int16_t>(input.data(), input.data()), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128s_decode<int16_t>(input.data(), input.data() + 1), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128s_decode<int16_t>(input), parser_error, m);
}
