#include "execute.hpp"
#include "parser.hpp"
#include "utils.hpp"
#include <gtest/gtest.h>

namespace
{
fizzy::execution_result execute_unary_operation(fizzy::instr instr, uint64_t arg)
{
    fizzy::module module;
    module.codesec.emplace_back(
        fizzy::code{0, {fizzy::instr::local_get, instr, fizzy::instr::end}, {0, 0, 0, 0}});

    return fizzy::execute(module, 0, {arg});
}

fizzy::execution_result execute_binary_operation(fizzy::instr instr, uint64_t lhs, uint64_t rhs)
{
    fizzy::module module;
    module.codesec.emplace_back(
        fizzy::code{0, {fizzy::instr::local_get, fizzy::instr::local_get, instr, fizzy::instr::end},
            {0, 0, 0, 0, 1, 0, 0, 0}});

    return fizzy::execute(module, 0, {rhs, lhs});
}
}  // namespace

TEST(execute, end)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0, {fizzy::instr::end}, {}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, unreachable)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0, {fizzy::instr::unreachable, fizzy::instr::end}, {}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_TRUE(trap);
}

TEST(execute, nop)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0, {fizzy::instr::nop, fizzy::instr::end}, {}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, drop)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{
        1, {fizzy::instr::local_get, fizzy::instr::drop, fizzy::instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
}

TEST(execute, select)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0,
        {fizzy::instr::local_get, fizzy::instr::local_get, fizzy::instr::local_get,
            fizzy::instr::select, fizzy::instr::end},
        {0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0}});

    auto result = fizzy::execute(module, 0, {3, 6, 0});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 6);

    result = fizzy::execute(module, 0, {3, 6, 1});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 3);

    result = fizzy::execute(module, 0, {3, 6, 42});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 3);
}

TEST(execute, local_get)
{
    fizzy::module module;
    module.codesec.emplace_back(
        fizzy::code{0, {fizzy::instr::local_get, fizzy::instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_set)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{1,
        {fizzy::instr::local_get, fizzy::instr::local_set, fizzy::instr::local_get,
            fizzy::instr::end},
        {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_tee)
{
    fizzy::module module;
    module.codesec.emplace_back(
        fizzy::code{1, {fizzy::instr::local_get, fizzy::instr::local_tee, fizzy::instr::end},
            {0, 0, 0, 0, 1, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_add)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_add, 22, 20);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_sub)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_sub, 424242, 424200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_mul)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_mul, 2, 21);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_div_s)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_div_s, uint64_t(-84), 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], -42);
}

TEST(execute, i32_div_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_div_s, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i32_div_s_overflow)
{
    const auto [trap, ret] = execute_binary_operation(
        fizzy::instr::i32_div_s, uint64_t(std::numeric_limits<int32_t>::min()), uint64_t(-1));

    ASSERT_TRUE(trap);
}

TEST(execute, i32_div_u)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_div_u, 84, 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_div_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_div_u, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i32_rem_s)
{
    const auto [trap, ret] =
        execute_binary_operation(fizzy::instr::i32_rem_s, uint64_t(-4242), 4200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], -42);
}

TEST(execute, i32_rem_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_rem_s, uint64_t(-4242), 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i32_rem_u)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_rem_u, 4242, 4200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_rem_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_rem_u, 4242, 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i32_and)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_and, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00);
}

TEST(execute, i32_or)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_or, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffff);
}

TEST(execute, i32_xor)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_xor, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00ff);
}

TEST(execute, i32_shl)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_shl, 21, 1);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_shr_s)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_shr_s, uint64_t(-84), 1);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], -42);
}

TEST(execute, i32_shr_u)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_shr_u, 84, 1);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_rotl)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_rotl, 0xff000000, 4);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xf000000f);
}

TEST(execute, i32_rotr)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::instr::i32_rotr, 0x000000ff, 4);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xf000000f);
}

TEST(execute, i32_wrap_i64)
{
    const auto [trap, ret] =
        execute_unary_operation(fizzy::instr::i32_wrap_i64, 0xffffffffffffffff);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffffff);
}

TEST(execute, i64_extend_i32_s_all_bits_set)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::instr::i64_extend_i32_s, 0xffffffff);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffffffffffffff);
}

TEST(execute, i64_extend_i32_s_one_bit_set)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::instr::i64_extend_i32_s, 0x80000000);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffffff80000000);
}

TEST(execute, i64_extend_i32_s_0)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::instr::i64_extend_i32_s, 0);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0);
}

TEST(execute, i64_extend_i32_s_1)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::instr::i64_extend_i32_s, 0x01);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x01);
}

TEST(execute, i64_extend_i32_u)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::instr::i64_extend_i32_u, 0xff000000);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x00000000ff000000);
}

TEST(execute, milestone1)
{
    /*
    (module
      (func $add (param $lhs i32) (param $rhs i32) (result i32)
        (local $local1 i32)
        local.get $lhs
        local.get $rhs
        i32.add
        local.get $local1
        i32.add
        local.tee $local1
        local.get $lhs
        i32.add
      )
    )
    */

    const auto bin = fizzy::from_hex(
        "0061736d0100000001070160027f7f017f030201000a13011101017f200020016a20026a220220006a0b");
    const auto module = fizzy::parse(bin);

    const auto [trap, ret] = fizzy::execute(module, 0, {20, 22});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 20 + 22 + 20);
}
