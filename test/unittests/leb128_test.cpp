#include "leb128.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;

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
    // clang-format off
    std::vector<std::pair<bytes, uint64_t>> testcases = {
        {{0}, 0}, // 0
        {{0x80, 0x80, 0x00}, 0}, // 0 with leading zeroes
        {{1}, 1}, // 1
        {{0x81, 0x80, 0x80, 0x00}, 1}, // 1 with leading zeroes
        {{0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 1}, // 1 with max leading zeroes
        {{0xe5, 0x8e, 0x26}, 624485}, // 624485
        {{0xe5, 0x8e, 0xa6, 0x80, 0x80, 0x00}, 624485}, // 624485 with leading zeroes
        {{0xff, 0xff, 0xff, 0xff, 0x07}, 0x7fffffff},
        {{0x80, 0x80, 0x80, 0x80, 0x08}, 0x80000000},
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00}, 562949953421311}, // bigger than int32
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x80, 0x00}, 562949953421311}, // bigger than int32 with zeroes
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}, 0x7fffffffffffffff},
        {{0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x01}, 0x8000000000000000},
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01}, std::numeric_limits<uint64_t>::max()}, // max
    };
    // clang-format on

    for (auto const& testcase : testcases)
    {
        auto res = leb128u_decode<uint64_t>(testcase.first);
        EXPECT_EQ(res.first, testcase.second) << hex(testcase.first);
        EXPECT_EQ(res.second, &testcase.first[0] + testcase.first.size()) << hex(testcase.first);
    }
}

TEST(leb128, decode_u64_invalid)
{
    bytes encoded_624485_invalid{0xe5, 0x8e, 0xa6};
    EXPECT_THROW_MESSAGE(
        leb128u_decode<uint64_t>(encoded_624485_invalid), parser_error, "Unexpected EOF");

    bytes encoded_1_too_many_leading_zeroes{
        0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(encoded_1_too_many_leading_zeroes), parser_error,
        "Invalid LEB128 encoding: too many bytes.");

    bytes encoded_max_leading_zeroes{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x81, 0x00};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(encoded_max_leading_zeroes), parser_error,
        "Invalid LEB128 encoding: too many bytes.");

    bytes encoded_max_unused_bits_set{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(encoded_max_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits set.");

    bytes encoded_max_some_unused_bits_set{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x19};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint64_t>(encoded_max_some_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits set.");
}

TEST(leb128, decode_u32)
{
    // clang-format off
    std::vector<std::pair<bytes, uint32_t>> testcases = {
        {{0}, 0}, // 0
        {{0x80, 0x80, 0x00}, 0}, // 0 with leading zeroes
        {{1}, 1}, // 1
        {{0x81, 0x80, 0x80, 0x00}, 1}, // 1 with leading zeroes
        {{0x81, 0x80, 0x80, 0x80, 0x00}, 1}, // 1 with max leading zeroes
        {{0x82, 0x00}, 2}, // 2 with leading zeroes
        {{0xe5, 0x8e, 0x26}, 624485}, // 624485
        {{0xe5, 0x8e, 0xa6, 0x80, 0x00}, 624485}, // 624485 with leading zeroes
        {{0xff, 0xff, 0xff, 0xff, 0x07}, 0x7fffffff},
        {{0x80, 0x80, 0x80, 0x80, 0x08}, 0x80000000},
        {{0xff, 0xff, 0xff, 0xff, 0x0f}, std::numeric_limits<uint32_t>::max()}, // max
    };
    // clang-format on

    for (auto const& testcase : testcases)
    {
        auto res = leb128u_decode<uint32_t>(testcase.first);
        EXPECT_EQ(res.first, testcase.second) << hex(testcase.first);
        EXPECT_EQ(res.second, &testcase.first[0] + testcase.first.size()) << hex(testcase.first);
    }
}

TEST(leb128, decode_u32_invalid)
{
    bytes encoded_624485_invalid{0xe5, 0x8e, 0xa6};
    EXPECT_THROW_MESSAGE(
        leb128u_decode<uint32_t>(encoded_624485_invalid), parser_error, "Unexpected EOF");

    bytes encoded_1_too_many_leading_zeroes{0x81, 0x80, 0x80, 0x80, 0x80, 0x00};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_1_too_many_leading_zeroes), parser_error,
        "Invalid LEB128 encoding: too many bytes.");

    bytes encoded_max_leading_zeroes{0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_max_leading_zeroes), parser_error,
        "Invalid LEB128 encoding: too many bytes.");

    bytes encoded_max_unused_bits_set{0xff, 0xff, 0xff, 0xff, 0x7f};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_max_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits set.");

    bytes encoded_2_unused_bits_set{0x82, 0x80, 0x80, 0x80, 0x70};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_2_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits set.");

    bytes encoded_0_some_unused_bits_set{0x80, 0x80, 0x80, 0x80, 0x1f};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint32_t>(encoded_0_some_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits set.");
}

TEST(leb128, decode_u8)
{
    // clang-format off
    std::vector<std::pair<bytes, uint64_t>> testcases = {
        {{0}, 0}, // 0
        {{0x80, 0x00}, 0}, // 0 with leading zero
        {{1}, 1}, // 1
        {{0x81, 0x00}, 1}, // 1 with leading zero
        {{0xe5, 0x01}, 229}, // 229
        {{0xff, 0x01}, 255}, // max
    };
    // clang-format on

    for (auto const& testcase : testcases)
    {
        auto res = leb128u_decode<uint8_t>(testcase.first);
        EXPECT_EQ(res.first, testcase.second) << hex(testcase.first);
        EXPECT_EQ(res.second, &testcase.first[0] + testcase.first.size()) << hex(testcase.first);
    }
}

TEST(leb128, decode_u8_invalid)
{
    bytes encoded_1_too_many_leading_zeroes{0x81, 0x80, 0x80};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(encoded_1_too_many_leading_zeroes.data()),
        parser_error, "Invalid LEB128 encoding: too many bytes.");

    bytes encoded_too_big{0xe5, 0x8e, 0x26};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(encoded_too_big.data()), parser_error,
        "Invalid LEB128 encoding: too many bytes.");

    bytes encoded_max_unused_bits_set{0xff, 0x7f};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(encoded_max_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits set.");

    bytes encoded_max_some_unused_bits_set{0xff, 0x19};
    EXPECT_THROW_MESSAGE(leb128u_decode<uint8_t>(encoded_max_some_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits set.");
}

TEST(leb128, decode_out_of_buffer)
{
    constexpr auto m = "Unexpected EOF";
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
    // clang-format off
    std::vector<std::pair<bytes, int64_t>> testcases = {
        {{0}, 0}, // 0
        {{0x80, 0x80, 0x00}, 0}, // 0 with leading zeroes
        {{1}, 1},
        {{0x81, 0x80, 0x80, 0x00}, 1}, // 1 with leading zeroes
        {{0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 1}, // 1 with max leading zeroes
        {{0x7f}, -1},
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}, -1}, // -1 with leading 1s
        {{0x7e}, -2},
        {{0xfe, 0x7f}, -2}, // -2 with leading 1s
        {{0xfe, 0xff, 0x7f}, -2},  // -2 with leading 1s
        {{0xe5, 0x8e, 0x26}, 624485},
        {{0xe5, 0x8e, 0xa6, 0x80, 0x80, 0x00}, 624485}, // 624485 with leading zeroes
        {{0xc0, 0xbb, 0x78}, -123456},
        {{0x9b, 0xf1, 0x59}, -624485},
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00}, 562949953421311}, // bigger than int32
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x80, 0x00}, 562949953421311}, // bigger than int32 with zeroes
    };
    // clang-format on

    for (auto const& testcase : testcases)
    {
        const auto res = leb128s_decode<int64_t>(testcase.first);
        EXPECT_EQ(res.first, testcase.second) << hex(testcase.first);
        EXPECT_EQ(res.second, &testcase.first[0] + testcase.first.size()) << hex(testcase.first);
    }
}

TEST(leb128, decode_s64_invalid)
{
    const bytes encoded_1_too_many_leading_zeroes{
        0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW_MESSAGE(leb128s_decode<int64_t>(encoded_1_too_many_leading_zeroes), parser_error,
        "Invalid LEB128 encoding: too many bytes.");

    const bytes encoded_minus1_too_many_leading_1s{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    EXPECT_THROW_MESSAGE(leb128s_decode<int64_t>(encoded_minus1_too_many_leading_1s), parser_error,
        "Invalid LEB128 encoding: too many bytes.");

    const bytes minus1_unused_bits_unset{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    EXPECT_THROW_MESSAGE(leb128s_decode<int64_t>(minus1_unused_bits_unset), parser_error,
        "Invalid LEB128 encoding: unused bits not equal to sign bit.");

    const bytes minus1_some_unused_bits_unset{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x79};
    EXPECT_THROW_MESSAGE(leb128s_decode<int64_t>(minus1_some_unused_bits_unset), parser_error,
        "Invalid LEB128 encoding: unused bits not equal to sign bit.");
}

TEST(leb128, decode_s32)
{
    // clang-format off
    std::vector<std::pair<bytes, int32_t>> testcases = {
        {{0}, 0}, // 0
        {{0x80, 0x80, 0x00}, 0}, // 0 with leading zeroes
        {{1}, 1},
        {{0x81, 0x80, 0x80, 0x00}, 1}, // 1 with leading zeroes
        {{0x81, 0x80, 0x80, 0x80, 0x00}, 1}, // 1 with max leading zeroes
        {{0x7f}, -1},
        {{0xff, 0xff, 0xff, 0xff, 0x7f}, -1}, // -1 with leading 1s
        {{0x7e}, -2},
        {{0xfe, 0x7f}, -2}, // -2 with leading 1s
        {{0xfe, 0xff, 0x7f}, -2}, // -2 with leading 1s
        {{0xe5, 0x8e, 0x26}, 624485},
        {{0xe5, 0x8e, 0xa6, 0x80, 0x00}, 624485}, // 624485 with leading zeroes
        {{0xc0, 0xbb, 0x78}, -123456},
        {{0x9b, 0xf1, 0x59}, -624485},
        {{0x81, 0x80, 0x80, 0x80, 0x78}, -2147483647},
        {{0x80, 0x80, 0x80, 0x80, 0x78}, std::numeric_limits<int32_t>::min()},
    };
    // clang-format on

    for (auto const& testcase : testcases)
    {
        const auto res = leb128s_decode<int32_t>(testcase.first);
        EXPECT_EQ(res.first, testcase.second) << hex(testcase.first);
        EXPECT_EQ(res.second, &testcase.first[0] + testcase.first.size()) << hex(testcase.first);
    }
}

TEST(leb128, decode_s32_invalid)
{
    const bytes encoded_0_unused_bits_set{0x80, 0x80, 0x80, 0x80, 0x70};
    EXPECT_THROW_MESSAGE(leb128s_decode<int32_t>(encoded_0_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits not equal to sign bit.");

    const bytes encoded_0_some_unused_bits_set{0x80, 0x80, 0x80, 0x80, 0x10};
    EXPECT_THROW_MESSAGE(leb128s_decode<int32_t>(encoded_0_some_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits not equal to sign bit.");

    const bytes minus1_unused_bits_unset{0xff, 0xff, 0xff, 0xff, 0x0f};
    EXPECT_THROW_MESSAGE(leb128s_decode<int32_t>(minus1_unused_bits_unset), parser_error,
        "Invalid LEB128 encoding: unused bits not equal to sign bit.");

    const bytes minus1_some_unused_bits_set{0xff, 0xff, 0xff, 0xff, 0x4f};
    EXPECT_THROW_MESSAGE(leb128s_decode<int32_t>(minus1_some_unused_bits_set), parser_error,
        "Invalid LEB128 encoding: unused bits not equal to sign bit.");
}

TEST(leb128, decode_s8)
{
    // clang-format off
    std::vector<std::pair<bytes, int64_t>> testcases = {
        {{0}, 0}, // 0
        {{0x80, 0x00}, 0}, // 0 with leading zero
        {{1}, 1}, // 1
        {{0x81, 0x00}, 1}, // 1 with leading zero
        {{0xff, 0x7f}, -1},
        {{0xfe, 0x7f}, -2},
        {{0x40}, -64},
        {{0x81, 0x7f}, -127},
        {{0x80, 0x7f}, std::numeric_limits<int8_t>::min()},
    };
    // clang-format on

    for (auto const& testcase : testcases)
    {
        const auto res = leb128s_decode<int8_t>(testcase.first);
        EXPECT_EQ(res.first, testcase.second) << hex(testcase.first);
        EXPECT_EQ(res.second, &testcase.first[0] + testcase.first.size()) << hex(testcase.first);
    }
}

TEST(leb128, decode_s8_invalid)
{
    const bytes encoded_1_too_many_leading_zeroes{0x81, 0x80, 0x80};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(encoded_1_too_many_leading_zeroes), parser_error,
        "Invalid LEB128 encoding: too many bytes.");

    const bytes encoded_too_big{0xe5, 0x8e, 0x26};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(encoded_too_big), parser_error,
        "Invalid LEB128 encoding: too many bytes.");

    const bytes minus1_unused_bits_unset{0xff, 0x01};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(minus1_unused_bits_unset), parser_error,
        "Invalid LEB128 encoding: unused bits not equal to sign bit.");

    const bytes minus2_unused_bits_unset{0xfe, 0x01};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(minus2_unused_bits_unset), parser_error,
        "Invalid LEB128 encoding: unused bits not equal to sign bit.");

    const bytes minus1_some_unused_bits_unset{0xff, 0x71};
    EXPECT_THROW_MESSAGE(leb128s_decode<int8_t>(minus1_some_unused_bits_unset), parser_error,
        "Invalid LEB128 encoding: unused bits not equal to sign bit.");
}

TEST(leb128, decode_s_out_of_buffer)
{
    constexpr auto m = "Unexpected EOF";
    const auto input = bytes{0x82, 0x81};

    EXPECT_THROW_MESSAGE(leb128s_decode<int16_t>(nullptr, nullptr), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128s_decode<int16_t>(input.data(), input.data()), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128s_decode<int16_t>(input.data(), input.data() + 1), parser_error, m);
    EXPECT_THROW_MESSAGE(leb128s_decode<int16_t>(input), parser_error, m);
}