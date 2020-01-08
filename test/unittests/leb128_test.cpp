#include "leb128.hpp"
#include "utils.hpp"
#include <gtest/gtest.h>

TEST(leb128, decode_u64)
{
    // clang-format off
    std::vector<std::pair<fizzy::bytes, uint64_t>> testcases = {
        {{0}, 0}, // 0
        {{0x80, 0x80, 0x00}, 0}, // 0 with leading zeroes
        {{1}, 1}, // 1
        {{0x81, 0x80, 0x80, 0x00}, 1}, // 1 with leading zeroes
        {{0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 1}, // 1 with max leading zeroes
        {{0xe5, 0x8e, 0x26}, 624485}, // 624485
        {{0xe5, 0x8e, 0xa6, 0x80, 0x80, 0x00}, 624485}, // 624485 with leading zeroes
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00}, 562949953421311}, // bigger than int32
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x80, 0x00}, 562949953421311}, // bigger than int32 with zeroes
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01}, std::numeric_limits<uint64_t>::max()}, // max
        // this is not really correct encoding, but is not rejected by our decoder
        {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f}, std::numeric_limits<uint64_t>::max()},
    };
    // clang-format on

    for (auto const& testcase : testcases)
    {
        auto res = fizzy::leb128u_decode<uint64_t>(testcase.first.data());
        EXPECT_EQ(res.first, testcase.second) << fizzy::hex(testcase.first);
        EXPECT_EQ(res.second, &testcase.first[0] + testcase.first.size())
            << fizzy::hex(testcase.first);
    }
}

TEST(leb128, decode_u64_invalid)
{
    // buffer overrun is not caught
    //    fizzy::bytes encoded_624485_invalid{0xe5, 0x8e, 0xa6};
    //    EXPECT_THROW(fizzy::leb128_decode_u64(encoded_624485_invalid.data()), std::runtime_error);

    fizzy::bytes encoded_1_too_many_leading_zeroes{
        0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW(fizzy::leb128u_decode<uint64_t>(encoded_1_too_many_leading_zeroes.data()),
        std::runtime_error);

    fizzy::bytes encoded_max_leading_zeroes{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x81, 0x00};
    EXPECT_THROW(
        fizzy::leb128u_decode<uint64_t>(encoded_max_leading_zeroes.data()), std::runtime_error);
}

TEST(leb128, decode_u8)
{
    // clang-format off
    std::vector<std::pair<fizzy::bytes, uint64_t>> testcases = {
        {{0}, 0}, // 0
        {{0x80, 0x00}, 0}, // 0 with leading zero
        {{1}, 1}, // 1
        {{0x81, 0x00}, 1}, // 1 with leading zero
        {{0xe5, 0x01}, 229}, // 229
        {{0xff, 0x01}, 255}, // max
        // this is not really correct encoding, but is not rejected by our decoder
        {{0xff, 0x7f}, 255},
    };
    // clang-format on

    for (auto const& testcase : testcases)
    {
        auto res = fizzy::leb128u_decode<uint8_t>(testcase.first.data());
        EXPECT_EQ(res.first, testcase.second) << fizzy::hex(testcase.first);
        EXPECT_EQ(res.second, &testcase.first[0] + testcase.first.size())
            << fizzy::hex(testcase.first);
    }
}

TEST(leb128, decode_u8_invalid)
{
    fizzy::bytes encoded_1_too_many_leading_zeroes{0x81, 0x80, 0x80};
    EXPECT_THROW(fizzy::leb128u_decode<uint8_t>(encoded_1_too_many_leading_zeroes.data()),
        std::runtime_error);

    fizzy::bytes encoded_too_big{0xe5, 0x8e, 0x26};
    EXPECT_THROW(fizzy::leb128u_decode<uint8_t>(encoded_too_big.data()), std::runtime_error);
}

TEST(leb128, decode_s64)
{
    // clang-format off
    std::vector<std::pair<fizzy::bytes, int64_t>> testcases = {
            {{0}, 0}, // 0
            {{0x80, 0x80, 0x00}, 0}, // 0 with leading zeroes
            {{1}, 1},
            {{0x81, 0x80, 0x80, 0x00}, 1}, // 1 with leading zeroes
            {{0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00}, 1}, // 1 with max leading zeroes
            {{0x7f}, -1},
            {{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01}, -1},
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
        auto res = fizzy::leb128s_decode<int64_t>(testcase.first.data());
        EXPECT_EQ(res.first, testcase.second) << fizzy::hex(testcase.first);
        EXPECT_EQ(res.second, &testcase.first[0] + testcase.first.size())
            << fizzy::hex(testcase.first);
    }
}
