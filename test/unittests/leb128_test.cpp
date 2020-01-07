#include "leb128.hpp"
#include <gtest/gtest.h>

TEST(leb128, decode_unsigned)
{
    fizzy::bytes encoded_0{0};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_0.data()).first, 0);

    fizzy::bytes encoded_0_leading_zeroes{0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_0_leading_zeroes.data()).first, 0);

    fizzy::bytes encoded_1{1};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_1.data()).first, 1);

    fizzy::bytes encoded_1_leading_zeroes{0x81, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_1_leading_zeroes.data()).first, 1);

    fizzy::bytes encoded_1_max_leading_zeroes{
        0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_1_max_leading_zeroes.data()).first, 1);

    fizzy::bytes encoded_624485{0xe5, 0x8e, 0x26};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_624485.data()).first, 624485);

    fizzy::bytes encoded_624485_leading_zeroes{0xe5, 0x8e, 0xa6, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_624485_leading_zeroes.data()).first, 624485);

    fizzy::bytes encoded_bigger_than_int32{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_bigger_than_int32.data()).first, 562949953421311);

    fizzy::bytes encoded_bigger_than_int32_leading_zeroes{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x80, 0x00};
    EXPECT_EQ(fizzy::leb128u_decode(encoded_bigger_than_int32_leading_zeroes.data()).first,
        562949953421311);

    fizzy::bytes encoded_max{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x01};
    EXPECT_EQ(
        fizzy::leb128u_decode(encoded_max.data()).first, std::numeric_limits<uint64_t>::max());

    // this is not really correct encoding, but is not rejected by our decoder
    fizzy::bytes encoded_max2{0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x7f};
    EXPECT_EQ(
        fizzy::leb128u_decode(encoded_max2.data()).first, std::numeric_limits<uint64_t>::max());
}

TEST(leb128, decode_unsigned_invalid)
{
    // buffer overrun is not caught
    //    fizzy::bytes encoded_624485_invalid{0xe5, 0x8e, 0xa6};
    //    EXPECT_THROW(fizzy::leb128u_decode(encoded_624485_invalid.data()), std::runtime_error);

    fizzy::bytes encoded_1_too_many_leading_zeroes{
        0x81, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80};
    EXPECT_THROW(
        fizzy::leb128u_decode(encoded_1_too_many_leading_zeroes.data()), std::runtime_error);

    fizzy::bytes encoded_max_leading_zeroes{
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x81, 0x00};
    EXPECT_THROW(fizzy::leb128u_decode(encoded_max_leading_zeroes.data()), std::runtime_error);
}
