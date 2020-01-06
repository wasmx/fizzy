#include "leb128.hpp"
#include <gtest/gtest.h>

TEST(leb128, decode_unsigned)
{
    fizzy::bytes encoded_0{0};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_0), 0);

    fizzy::bytes encoded_0_leading_zeroes{0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_0_leading_zeroes), 0);

    fizzy::bytes encoded_1{1};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_1), 1);

    fizzy::bytes encoded_1_leading_zeroes{0x81, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_1_leading_zeroes), 1);

    fizzy::bytes encoded_1_max_leading_zeroes{0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW(fizzy::leb128u_decode(encoded_1_max_leading_zeroes), std::runtime_error);

    fizzy::bytes encoded_624485{0xe5, 0x8e, 0x26};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_624485), 624485);

    fizzy::bytes encoded_624485_leading_zeroes{0xe5, 0x8e, 0xa6, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_624485_leading_zeroes), 624485);

    fizzy::bytes encoded_bigger_than_int32{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_bigger_than_int32), 562949953421311);

    fizzy::bytes encoded_bigger_than_int32_leading_zeroes{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_bigger_than_int32_leading_zeroes), 562949953421311);

    fizzy::bytes encoded_max{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_max), std::numeric_limits<uint64_t>::max());
}

TEST(leb128, decode_unsigned_invalid)
{
    fizzy::bytes encoded_624485_invalid{0xe5, 0x8e, 0xa6};
    EXPECT_THROW(fizzy::leb128u_decode(encoded_624485_invalid), std::runtime_error);

    fizzy::bytes encoded_1_too_many_leading_zeroes{
        0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW(fizzy::leb128u_decode(encoded_1_too_many_leading_zeroes), std::runtime_error);

    fizzy::bytes encoded_max_leading_zeroes{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x81, 0x00};
    EXPECT_THROW(fizzy::leb128u_decode(encoded_max_leading_zeroes), std::runtime_error);
}
