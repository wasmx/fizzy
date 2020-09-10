// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "span.hpp"
#include "stack.hpp"
#include <gtest/gtest.h>
#include <array>
#include <vector>

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
    EXPECT_EQ(*s.data(), 2);
    EXPECT_EQ(*s.begin(), 2);
    EXPECT_EQ(*(s.end() - 1), 4);

    vec[1] = 100;
    EXPECT_EQ(s[0], 100);

    const span<const uint64_t> s2 = vec;  // Implicit conversion from vector.
    for (size_t i = 0; i < vec.size(); ++i)
        EXPECT_EQ(s2[i], vec[i]);
}

TEST(span, array)
{
    float a1[] = {1, 2, 3};
    span<const float> s1 = a1;
    EXPECT_EQ(s1.size(), 3);
    EXPECT_EQ(s1[0], 1.0f);
    EXPECT_EQ(s1[1], 2.0f);
    EXPECT_EQ(s1[2], 3.0f);

    const std::array a2 = {0.1f, 0.2f, 0.3f};
    span<const float> s2 = a2;
    EXPECT_EQ(s2.size(), 3);
    EXPECT_EQ(s2[0], 0.1f);
    EXPECT_EQ(s2[1], 0.2f);
    EXPECT_EQ(s2[2], 0.3f);
}

TEST(span, stack)
{
    OperandStack stack({}, 0, 4, nullptr, 0);
    stack.push(10);
    stack.push(11);
    stack.push(12);
    stack.push(13);

    constexpr auto num_items = 2;
    span<const Value> s(stack.rend() - num_items, num_items);
    EXPECT_EQ(s.size(), 2);
    EXPECT_EQ(s[0].i64, 12);
    EXPECT_EQ(s[1].i64, 13);

    stack[0] = 0;
    EXPECT_EQ(s[1].i64, 0);
}

TEST(span, initializer_list)
{
    // This only works for lvalue initializer_lists, but not as `span{1, 2, 3}`.
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

TEST(span, iterator)
{
    std::string str{"__abc__"};
    span<const char> slice{str.data() + 2, 3};

    auto it = slice.begin();
    EXPECT_EQ(&*it, slice.data());
    EXPECT_EQ(*it, 'a');
    ++it;
    EXPECT_EQ(*it, 'b');
    ++it;
    EXPECT_EQ(*it, 'c');
    ++it;
    EXPECT_EQ(it, slice.end());

    EXPECT_EQ(slice.end() - slice.begin(), slice.size());
}

TEST(span, iterator_range)
{
    std::string str{"__abc__"};
    span<const char> sp = str;

    std::string copy;
    std::copy(std::begin(sp), std::end(sp), std::back_inserter(copy));
    EXPECT_EQ(copy, str);
}

TEST(span, for_range)
{
    std::string str{"**xyz**"};
    span<const char> sp = str;

    std::string copy;
    for (const auto c : sp)
        copy.push_back(c);

    EXPECT_EQ(copy, str);
}

TEST(span, reverse_iterator)
{
    int a[] = {1, 2, 3, 4, 5, 6};
    span<const int> s{&a[1], 4};

    auto it = s.rbegin();
    EXPECT_EQ(*it, 5);
    ++it;
    EXPECT_EQ(*it, 4);
    ++it;
    EXPECT_EQ(*it, 3);
    ++it;
    EXPECT_EQ(*it, 2);
    ++it;
    EXPECT_EQ(it, s.rend());

    EXPECT_EQ(s.rend() - s.rbegin(), s.size());
}
