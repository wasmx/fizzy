// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "constexpr_vector.hpp"
#include "cxx20/span.hpp"
#include <gmock/gmock.h>
#include <vector>

using namespace fizzy;
using namespace testing;

static_assert(constexpr_vector<int, 5>{1, 2, 3}.size() == 3);
static_assert(*constexpr_vector<int, 5>{1, 2, 3}.data() == 1);

TEST(constexpr_vector, size)
{
    constexpr constexpr_vector<int, 5> v1 = {};
    EXPECT_EQ(v1.size(), 0);

    constexpr constexpr_vector<int, 5> v2 = {1, 2, 3};
    EXPECT_EQ(v2.size(), 3);
    EXPECT_EQ(*v2.data(), 1);

    constexpr constexpr_vector<int, 5> v3 = {1, 2, 3, 4, 5};
    EXPECT_EQ(v3.size(), 5);
    EXPECT_EQ(*v3.data(), 1);
}

TEST(constexpr_vector, subscript)
{
    constexpr constexpr_vector<int, 3> v1 = {1, 2, 3};
    ASSERT_EQ(v1.size(), 3);
    EXPECT_EQ(v1[0], 1);
    EXPECT_EQ(v1[1], 2);
    EXPECT_EQ(v1[2], 3);

    constexpr constexpr_vector<int, 4> v2 = {1, 2, 3};
    ASSERT_EQ(v2.size(), 3);
    EXPECT_EQ(v2[0], 1);
    EXPECT_EQ(v2[1], 2);
    EXPECT_EQ(v2[2], 3);
}

TEST(constexpr_vector, iterator)
{
    constexpr constexpr_vector<int, 5> v = {1, 2, 3};
    int expected = 1;
    for (const auto i : v)
    {
        EXPECT_EQ(i, expected);
        ++expected;
    }
    EXPECT_EQ(expected, 4);
}

TEST(constexpr_vector, span)
{
    constexpr constexpr_vector<int, 5> v = {1, 2, 3};
    int expected = 1;
    for (const auto i : span<const int>(v))
    {
        EXPECT_EQ(i, expected);
        ++expected;
    }
    EXPECT_EQ(expected, 4);
}

TEST(constexpr_vector, array)
{
    constexpr constexpr_vector<int, 5> arr[] = {{1, 2, 3}, {4, 5, 6, 7, 8}, {9}, {}};
    EXPECT_THAT(std::vector<int>(arr[0].begin(), arr[0].end()), ElementsAre(1, 2, 3));
    EXPECT_THAT(std::vector<int>(arr[1].begin(), arr[1].end()), ElementsAre(4, 5, 6, 7, 8));
    EXPECT_THAT(std::vector<int>(arr[2].begin(), arr[2].end()), ElementsAre(9));
    EXPECT_THAT(std::vector<int>(arr[3].begin(), arr[3].end()), IsEmpty());
}
