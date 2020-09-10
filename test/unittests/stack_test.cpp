// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "stack.hpp"
#include <gmock/gmock.h>

using namespace fizzy;
using namespace testing;

TEST(stack, push_and_pop)
{
    Stack<char> stack;

    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());

    stack.push('a');
    stack.push('b');
    stack.push('c');

    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.pop(), 'c');
    EXPECT_EQ(stack.pop(), 'b');
    EXPECT_EQ(stack.pop(), 'a');

    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());
}

TEST(stack, emplace)
{
    Stack<char> stack;

    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());

    stack.emplace('a');
    stack.emplace('b');
    stack.emplace('c');

    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.pop(), 'c');
    EXPECT_EQ(stack.pop(), 'b');
    EXPECT_EQ(stack.pop(), 'a');

    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());
}

TEST(stack, shrink)
{
    Stack<char> stack;
    stack.push('a');
    stack.push('b');
    stack.push('c');
    stack.push('d');
    EXPECT_EQ(stack.top(), 'd');
    EXPECT_EQ(stack.size(), 4);

    stack.shrink(4);
    EXPECT_EQ(stack.top(), 'd');
    EXPECT_EQ(stack.size(), 4);

    stack.shrink(2);
    EXPECT_EQ(stack.top(), 'b');
    EXPECT_EQ(stack.size(), 2);

    stack.shrink(0);
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);
}

TEST(stack, struct_item)
{
    struct StackItem
    {
        char a, b, c;
        StackItem() = default;  // required for drop() (which calls resize())
        StackItem(char _a, char _b, char _c) : a(_a), b(_b), c(_c) {}
    };

    Stack<StackItem> stack;

    stack.emplace('a', 'b', 'c');
    stack.emplace('d', 'e', 'f');
    stack.emplace('g', 'h', 'i');

    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.top().a, 'g');
    EXPECT_EQ(stack.top().b, 'h');
    EXPECT_EQ(stack.top().c, 'i');
    EXPECT_EQ(stack[1].a, 'd');
    EXPECT_EQ(stack[1].b, 'e');
    EXPECT_EQ(stack[1].c, 'f');
    EXPECT_EQ(stack[2].a, 'a');
    EXPECT_EQ(stack[2].b, 'b');
    EXPECT_EQ(stack[2].c, 'c');

    EXPECT_EQ(stack.pop().a, 'g');

    EXPECT_EQ(stack.top().a, 'd');
    EXPECT_EQ(stack.top().b, 'e');
    EXPECT_EQ(stack.top().c, 'f');
    EXPECT_EQ(stack[1].a, 'a');
    EXPECT_EQ(stack[1].b, 'b');
    EXPECT_EQ(stack[1].c, 'c');
}


TEST(operand_stack, construct)
{
    OperandStack stack({}, 0, 0);
    EXPECT_EQ(stack.size(), 0);
}

TEST(operand_stack, top)
{
    OperandStack stack({}, 0, 1);
    EXPECT_EQ(stack.size(), 0);

    stack.push(1);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top().i64, 1);
    EXPECT_EQ(stack[0].i64, 1);

    stack.top() = 101;
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top().i64, 101);
    EXPECT_EQ(stack[0].i64, 101);

    stack.drop(1);
    EXPECT_EQ(stack.size(), 0);

    stack.push(2);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top().i64, 2);
    EXPECT_EQ(stack[0].i64, 2);
}

TEST(operand_stack, small)
{
    OperandStack stack({}, 0, 3);
    EXPECT_EQ(stack.size(), 0);

    stack.push(1);
    stack.push(2);
    stack.push(3);
    EXPECT_EQ(stack.size(), 3);
    EXPECT_EQ(stack.top().i64, 3);
    EXPECT_EQ(stack[0].i64, 3);
    EXPECT_EQ(stack[1].i64, 2);
    EXPECT_EQ(stack[2].i64, 1);

    stack[0] = 13;
    stack[1] = 12;
    stack[2] = 11;
    EXPECT_EQ(stack.size(), 3);
    EXPECT_EQ(stack.top().i64, 13);
    EXPECT_EQ(stack[0].i64, 13);
    EXPECT_EQ(stack[1].i64, 12);
    EXPECT_EQ(stack[2].i64, 11);

    EXPECT_EQ(stack.pop().i64, 13);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.top().i64, 12);
}

TEST(operand_stack, large)
{
    constexpr auto max_height = 33;
    OperandStack stack({}, 0, max_height);

    for (unsigned i = 0; i < max_height; ++i)
        stack.push(i);

    EXPECT_EQ(stack.size(), max_height);
    for (int expected = max_height - 1; expected >= 0; --expected)
        EXPECT_EQ(stack.pop().i64, expected);
    EXPECT_EQ(stack.size(), 0);
}

TEST(operand_stack, rbegin_rend)
{
    OperandStack stack({}, 0, 3);
    EXPECT_EQ(stack.rbegin(), stack.rend());

    stack.push(1);
    stack.push(2);
    stack.push(3);
    EXPECT_LT(stack.rbegin(), stack.rend());
    EXPECT_EQ(stack.rbegin()->i64, 1);
    EXPECT_EQ((stack.rend() - 1)->i64, 3);
}

TEST(operand_stack, to_vector)
{
    OperandStack stack({}, 0, 3);
    EXPECT_THAT(std::vector(stack.rbegin(), stack.rend()), IsEmpty());

    stack.push(1);
    stack.push(2);
    stack.push(3);

    auto const result = std::vector(stack.rbegin(), stack.rend());
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].i64, 1);
    EXPECT_EQ(result[1].i64, 2);
    EXPECT_EQ(result[2].i64, 3);
}
