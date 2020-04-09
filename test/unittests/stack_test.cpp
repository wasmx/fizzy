#include "stack.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

TEST(stack, push_and_pop)
{
    Stack<char> stack;

    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());

    stack.push('a');
    stack.push('b');
    stack.push('c');

    EXPECT_EQ(stack.size(), 3);

    EXPECT_EQ(stack.pop(), 'c');
    EXPECT_EQ(stack.pop(), 'b');
    EXPECT_EQ(stack.pop(), 'a');

    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());
}

TEST(stack, drop_and_peek)
{
    Stack<char> stack;
    stack.push('w');
    stack.push('x');
    stack.push('y');
    stack.push('z');

    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 4);
    EXPECT_EQ(stack.top(), 'z');
    EXPECT_EQ(stack[0], 'z');
    EXPECT_EQ(stack[1], 'y');
    EXPECT_EQ(stack[2], 'x');
    EXPECT_EQ(stack[3], 'w');
    EXPECT_EQ(stack.size(), 4);

    stack.drop();
    EXPECT_EQ(stack.size(), 3);
    EXPECT_EQ(stack.top(), 'y');

    stack.drop(2);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top(), 'w');

    stack.drop();
    EXPECT_EQ(stack.size(), 0);
    EXPECT_TRUE(stack.empty());
}

TEST(stack, clear)
{
    Stack<char> stack;
    stack.push('a');
    stack.push('b');

    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 2);

    stack.drop(0);
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 2);

    stack.clear();
    EXPECT_TRUE(stack.empty());
    EXPECT_EQ(stack.size(), 0);
}

TEST(stack, resize)
{
    Stack<char> stack;
    stack.push('a');
    stack.push('b');

    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 2);

    // grow stack
    stack.resize(4);
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.top(), 0);
    EXPECT_EQ(stack[1], 0);
    EXPECT_EQ(stack[2], 'b');
    EXPECT_EQ(stack[3], 'a');
    EXPECT_EQ(stack.size(), 4);

    stack.drop();
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 3);

    // shrink stack
    stack.resize(1);
    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 1);

    stack.clear();
    EXPECT_TRUE(stack.empty());
}

TEST(stack, iterator)
{
    Stack<char> stack;
    stack.push('a');
    stack.push('b');
    stack.push('c');

    EXPECT_FALSE(stack.empty());
    EXPECT_EQ(stack.size(), 3);

    auto bottom_item = stack.begin();
    EXPECT_EQ(*bottom_item, 'a');

    auto top_item = stack.end() - 1;
    EXPECT_EQ(*top_item, 'c');

    for (auto it = stack.begin(); it < stack.end(); it++)
        *it = 'x';

    for (unsigned item = 0; item < stack.size(); item++)
        EXPECT_EQ(stack[item], 'x');
}

TEST(stack, clear_on_empty)
{
    Stack<char> stack;
    stack.clear();
}
