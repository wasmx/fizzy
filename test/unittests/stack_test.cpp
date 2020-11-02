// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "stack.hpp"
#include <gmock/gmock.h>

using namespace fizzy;
using namespace testing;

namespace
{
intptr_t address_diff(const void* a, const void* b) noexcept
{
    return std::abs(reinterpret_cast<intptr_t>(a) - reinterpret_cast<intptr_t>(b));
}
}  // namespace

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
    OperandStack stack(nullptr, 0, 0, 0);
    EXPECT_EQ(stack.size(), 0);
}

TEST(operand_stack, top)
{
    OperandStack stack(nullptr, 0, 0, 1);
    EXPECT_EQ(stack.size(), 0);

    stack.push(1);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top().i32, 1);
    EXPECT_EQ(stack[0].i32, 1);

    stack.top() = 101;
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top().i32, 101);
    EXPECT_EQ(stack[0].i32, 101);

    stack.drop(0);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top().i32, 101);
    EXPECT_EQ(stack[0].i32, 101);

    stack.drop(1);
    EXPECT_EQ(stack.size(), 0);

    stack.push(2);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top().i32, 2);
    EXPECT_EQ(stack[0].i32, 2);
}

TEST(operand_stack, small)
{
    OperandStack stack(nullptr, 0, 0, 3);
    ASSERT_LT(address_diff(&stack, stack.rbegin()), 100) << "not allocated on the system stack";

    EXPECT_EQ(stack.size(), 0);

    stack.push(1);
    stack.push(2);
    stack.push(3);
    EXPECT_EQ(stack.size(), 3);
    EXPECT_EQ(stack.top().i32, 3);
    EXPECT_EQ(stack[0].i32, 3);
    EXPECT_EQ(stack[1].i32, 2);
    EXPECT_EQ(stack[2].i32, 1);

    stack[0] = 13;
    stack[1] = 12;
    stack[2] = 11;
    EXPECT_EQ(stack.size(), 3);
    EXPECT_EQ(stack.top().i32, 13);
    EXPECT_EQ(stack[0].i32, 13);
    EXPECT_EQ(stack[1].i32, 12);
    EXPECT_EQ(stack[2].i32, 11);

    EXPECT_EQ(stack.pop().i32, 13);
    EXPECT_EQ(stack.size(), 2);
    EXPECT_EQ(stack.top().i32, 12);
}

TEST(operand_stack, small_with_locals)
{
    const fizzy::Value args[] = {0xa1, 0xa2};
    OperandStack stack(args, std::size(args), 3, 1);
    ASSERT_LT(address_diff(&stack, stack.rbegin()), 100) << "not allocated on the system stack";

    EXPECT_EQ(stack.size(), 0);

    stack.push(0xff);
    EXPECT_EQ(stack.size(), 1);
    EXPECT_EQ(stack.top().i32, 0xff);
    EXPECT_EQ(stack[0].i32, 0xff);

    EXPECT_EQ(stack.local(0).i32, 0xa1);
    EXPECT_EQ(stack.local(1).i32, 0xa2);
    EXPECT_EQ(stack.local(2).i32, 0);
    EXPECT_EQ(stack.local(3).i32, 0);
    EXPECT_EQ(stack.local(4).i32, 0);

    stack.local(0) = 0xc0;
    stack.local(1) = 0xc1;
    stack.local(2) = 0xc2;
    stack.local(3) = 0xc3;
    stack.local(4) = 0xc4;

    EXPECT_EQ(stack.local(0).i32, 0xc0);
    EXPECT_EQ(stack.local(1).i32, 0xc1);
    EXPECT_EQ(stack.local(2).i32, 0xc2);
    EXPECT_EQ(stack.local(3).i32, 0xc3);
    EXPECT_EQ(stack.local(4).i32, 0xc4);

    EXPECT_EQ(stack.pop().i32, 0xff);
    EXPECT_EQ(stack.size(), 0);
    EXPECT_EQ(stack.local(0).i32, 0xc0);
    EXPECT_EQ(stack.local(1).i32, 0xc1);
    EXPECT_EQ(stack.local(2).i32, 0xc2);
    EXPECT_EQ(stack.local(3).i32, 0xc3);
    EXPECT_EQ(stack.local(4).i32, 0xc4);
}

TEST(operand_stack, large)
{
    constexpr auto max_height = 33;
    OperandStack stack(nullptr, 0, 0, max_height);
    ASSERT_GT(address_diff(&stack, stack.rbegin()), 100) << "not allocated on the heap";
    EXPECT_EQ(stack.size(), 0);

    for (unsigned i = 0; i < max_height; ++i)
        stack.push(i);

    EXPECT_EQ(stack.size(), max_height);
    for (int expected = max_height - 1; expected >= 0; --expected)
        EXPECT_EQ(stack.pop().i32, expected);
    EXPECT_EQ(stack.size(), 0);
}

TEST(operand_stack, large_with_locals)
{
    const fizzy::Value args[] = {0xa1, 0xa2};
    constexpr auto max_height = 33;
    constexpr auto num_locals = 5;
    constexpr auto num_args = std::size(args);
    OperandStack stack(args, num_args, num_locals, max_height);
    ASSERT_GT(address_diff(&stack, stack.rbegin()), 100) << "not allocated on the heap";

    for (unsigned i = 0; i < max_height; ++i)
        stack.push(i);

    EXPECT_EQ(stack.size(), max_height);
    for (unsigned i = 0; i < max_height; ++i)
        EXPECT_EQ(stack[i].i32, max_height - i - 1);

    EXPECT_EQ(stack.local(0).i32, 0xa1);
    EXPECT_EQ(stack.local(1).i32, 0xa2);

    for (unsigned i = num_args; i < num_args + num_locals; ++i)
        EXPECT_EQ(stack.local(i).i32, 0);

    for (unsigned i = 0; i < num_args + num_locals; ++i)
        stack.local(i) = fizzy::Value{i};
    for (unsigned i = 0; i < num_args + num_locals; ++i)
        EXPECT_EQ(stack.local(i).i32, i);

    for (int expected = max_height - 1; expected >= 0; --expected)
        EXPECT_EQ(stack.pop().i32, expected);
    EXPECT_EQ(stack.size(), 0);

    for (unsigned i = 0; i < num_args + num_locals; ++i)
        EXPECT_EQ(stack.local(i).i32, i);
}


TEST(operand_stack, rbegin_rend)
{
    OperandStack stack(nullptr, 0, 0, 3);
    EXPECT_EQ(stack.rbegin(), stack.rend());

    stack.push(1);
    stack.push(2);
    stack.push(3);
    EXPECT_LT(stack.rbegin(), stack.rend());
    EXPECT_EQ(stack.rbegin()->i32, 1);
    EXPECT_EQ((stack.rend() - 1)->i32, 3);
}

TEST(operand_stack, rbegin_rend_locals)
{
    const fizzy::Value args[] = {0xa1};
    OperandStack stack(args, std::size(args), 4, 2);
    EXPECT_EQ(stack.rbegin(), stack.rend());

    stack.push(1);
    EXPECT_LT(stack.rbegin(), stack.rend());
    EXPECT_EQ(stack.rend() - stack.rbegin(), 1);
    EXPECT_EQ(stack.rbegin()->i32, 1);
    EXPECT_EQ((stack.rend() - 1)->i32, 1);

    stack.push(2);
    EXPECT_LT(stack.rbegin(), stack.rend());
    EXPECT_EQ(stack.rend() - stack.rbegin(), 2);
    EXPECT_EQ(stack.rbegin()->i32, 1);
    EXPECT_EQ((stack.rbegin() + 1)->i32, 2);
    EXPECT_EQ((stack.rend() - 1)->i32, 2);
    EXPECT_EQ((stack.rend() - 2)->i32, 1);
}

TEST(operand_stack, to_vector)
{
    OperandStack stack(nullptr, 0, 0, 3);
    EXPECT_THAT(std::vector(stack.rbegin(), stack.rend()), IsEmpty());

    stack.push(1);
    stack.push(2);
    stack.push(3);

    auto const result = std::vector(stack.rbegin(), stack.rend());
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0].i32, 1);
    EXPECT_EQ(result[1].i32, 2);
    EXPECT_EQ(result[2].i32, 3);
}

TEST(operand_stack, hidden_stack_item)
{
    constexpr auto max_height = 33;
    OperandStack stack(nullptr, 0, 0, max_height);
    ASSERT_GT(address_diff(&stack, stack.rbegin()), 100) << "not allocated on the heap";
    EXPECT_EQ(stack.size(), 0);
    EXPECT_EQ(stack.rbegin(), stack.rend());

    // Even when stack is empty, there exists a single hidden item slot.
    const_cast<fizzy::Value*>(stack.rbegin())->i64 = 1;
    EXPECT_EQ(stack.rbegin()->i64, 1);
    EXPECT_EQ(stack.rend()->i64, 1);
}
