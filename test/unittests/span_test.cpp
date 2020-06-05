// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "span.hpp"
#include "stack.hpp"
#include <gtest/gtest.h>

using namespace fizzy;
using namespace testing;

TEST(span, vector)
{
    std::vector<uint64_t> vec{1, 2, 3, 4, 5, 6};
    span<const uint64_t> s(&vec[1], 3);
    EXPECT_EQ(s.size(), 3);
    EXPECT_EQ(s[0], 2);
    EXPECT_EQ(s[1], 3);
    EXPECT_EQ(s[2], 4);
    EXPECT_EQ(*s.begin(), 2);
    EXPECT_EQ(*(s.end() - 1), 4);

    vec[1] = 100;
    EXPECT_EQ(s[0], 100);

    const auto s2 = span<const uint64_t>(vec);
    for (size_t i = 0; i < vec.size(); ++i)
        EXPECT_EQ(s2[i], vec[i]);
}

TEST(span, stack)
{
    OperandStack stack(4);
    stack.push(10);
    stack.push(11);
    stack.push(12);
    stack.push(13);

    constexpr auto num_items = 2;
    span<const uint64_t> s(stack.rend() - num_items, num_items);
    EXPECT_EQ(s.size(), 2);
    EXPECT_EQ(s[0], 12);
    EXPECT_EQ(s[1], 13);

    stack[0] = 0;
    EXPECT_EQ(s[1], 0);
}

TEST(span, initializer_list)
{
    // Dangerous usage because user need to keep the initializer_list alive
    // as long as span is being used.
    const std::initializer_list<uint64_t> init = {1, 2, 3};
    const auto s = span<const uint64_t>(init);
    EXPECT_EQ(s.size(), 3);
    EXPECT_EQ(s[0], 1);
    EXPECT_EQ(s[1], 2);
    EXPECT_EQ(s[2], 3);

    // For range loop also works.
    uint64_t i = 0;
    for (const auto& x : s)
        EXPECT_EQ(x, ++i);
}
