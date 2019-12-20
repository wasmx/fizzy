#include "leb128.hpp"
#include <gtest/gtest.h>

TEST(leb128, DISABLED_decode)
{
    uint8_t encoded_1 = 1;
    EXPECT_EQ(fizzy::leb128_decode(&encoded_1), 1);
}