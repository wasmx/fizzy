#include "leb128.hpp"
#include <gtest/gtest.h>

TEST(leb128, decode_unsigned)
{
    uint8_t encoded_0 = 0;
    EXPECT_EQ(fizzy::leb128u_decode(&encoded_0), 0);

    uint8_t encoded_0_leading_zeroes[] = {0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_0_leading_zeroes), 0);

    uint8_t encoded_1 = 1;
    EXPECT_EQ(fizzy::leb128u_decode(&encoded_1), 1);

    uint8_t encoded_1_leading_zeroes[] = {0x81, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_1_leading_zeroes), 1);

    uint8_t encoded_624485[] = {0xe5, 0x8e, 0x26};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_624485), 624485);

    uint8_t encoded_624485_leading_zeroes[] = {0xe5, 0x8e, 0xa6, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_624485_leading_zeroes), 624485);

    uint8_t encoded_bigger_than_int32[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_bigger_than_int32), 562949953421311);

    uint8_t encoded_bigger_than_int32_leading_zeroes[] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_bigger_than_int32_leading_zeroes), 562949953421311);

    uint8_t encoded_max[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_max), std::numeric_limits<uint64_t>::max());

    uint8_t encoded_max_leading_zeroes[] = {
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x81, 0x80, 0x80, 0x00};
    EXPECT_EQ(
        fizzy::leb128u_decode(encoded_max_leading_zeroes), std::numeric_limits<uint64_t>::max());
}
