#include "leb128.hpp"
#include <gtest/gtest.h>

TEST(leb128, decode_unsigned)
{
    uint8_t encoded_0 = 0;
    EXPECT_EQ(fizzy::leb128u_decode(&encoded_0, 1), 0);

    uint8_t encoded_0_leading_zeroes[] = {0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_0_leading_zeroes, 3), 0);

    uint8_t encoded_1 = 1;
    EXPECT_EQ(fizzy::leb128u_decode(&encoded_1, 1), 1);

    uint8_t encoded_1_leading_zeroes[] = {0x81, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_1_leading_zeroes, 4), 1);

    uint8_t encoded_1_max_leading_zeroes[] = {0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW(fizzy::leb128u_decode(encoded_1_max_leading_zeroes, 9), std::runtime_error);

    uint8_t encoded_624485[] = {0xe5, 0x8e, 0x26};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_624485, 3), 624485);

    uint8_t encoded_624485_leading_zeroes[] = {0xe5, 0x8e, 0xa6, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_624485_leading_zeroes, 6), 624485);

    uint8_t encoded_bigger_than_int32[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_bigger_than_int32, 8), 562949953421311);

    uint8_t encoded_bigger_than_int32_leading_zeroes[] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_bigger_than_int32_leading_zeroes, 10), 562949953421311);

    uint8_t encoded_max[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_max, 10), std::numeric_limits<uint64_t>::max());
}

TEST(leb128, decode_unsigned_invalid)
{
    uint8_t encoded_624485_invalid[] = {0xe5, 0x8e, 0xa6};
    EXPECT_THROW(fizzy::leb128u_decode(encoded_624485_invalid, 3), std::runtime_error);

    uint8_t encoded_1_too_many_leading_zeroes[] = {
        0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW(fizzy::leb128u_decode(encoded_1_too_many_leading_zeroes, 10), std::runtime_error);

    uint8_t encoded_max_leading_zeroes[] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x81, 0x00};
    EXPECT_THROW(fizzy::leb128u_decode(encoded_max_leading_zeroes, 11), std::runtime_error);
}
