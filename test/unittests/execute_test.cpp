#include "execute.hpp"
#include "parser.hpp"
#include "utils.hpp"
#include <gtest/gtest.h>

namespace
{
fizzy::execution_result execute_unary_operation(fizzy::Instr instr, uint64_t arg)
{
    fizzy::Module module;
    module.codesec.emplace_back(
        fizzy::Code{0, {fizzy::Instr::local_get, instr, fizzy::Instr::end}, {0, 0, 0, 0}});

    return fizzy::execute(module, 0, {arg});
}

fizzy::execution_result execute_binary_operation(fizzy::Instr instr, uint64_t lhs, uint64_t rhs)
{
    fizzy::Module module;
    module.codesec.emplace_back(
        fizzy::Code{0, {fizzy::Instr::local_get, fizzy::Instr::local_get, instr, fizzy::Instr::end},
            {0, 0, 0, 0, 1, 0, 0, 0}});

    return fizzy::execute(module, 0, {rhs, lhs});
}
}  // namespace

TEST(execute, end)
{
    fizzy::Module module;
    module.codesec.emplace_back(fizzy::Code{0, {fizzy::Instr::end}, {}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, unreachable)
{
    fizzy::Module module;
    module.codesec.emplace_back(fizzy::Code{0, {fizzy::Instr::unreachable, fizzy::Instr::end}, {}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_TRUE(trap);
}

TEST(execute, nop)
{
    fizzy::Module module;
    module.codesec.emplace_back(fizzy::Code{0, {fizzy::Instr::nop, fizzy::Instr::end}, {}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, drop)
{
    fizzy::Module module;
    module.codesec.emplace_back(fizzy::Code{
        1, {fizzy::Instr::local_get, fizzy::Instr::drop, fizzy::Instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
}

TEST(execute, select)
{
    fizzy::Module module;
    module.codesec.emplace_back(fizzy::Code{0,
        {fizzy::Instr::local_get, fizzy::Instr::local_get, fizzy::Instr::local_get,
            fizzy::Instr::select, fizzy::Instr::end},
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
    fizzy::Module module;
    module.codesec.emplace_back(
        fizzy::Code{0, {fizzy::Instr::local_get, fizzy::Instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_set)
{
    fizzy::Module module;
    module.codesec.emplace_back(fizzy::Code{1,
        {fizzy::Instr::local_get, fizzy::Instr::local_set, fizzy::Instr::local_get,
            fizzy::Instr::end},
        {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_tee)
{
    fizzy::Module module;
    module.codesec.emplace_back(
        fizzy::Code{1, {fizzy::Instr::local_get, fizzy::Instr::local_tee, fizzy::Instr::end},
            {0, 0, 0, 0, 1, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_const)
{
    fizzy::Module module;
    module.codesec.emplace_back(
        fizzy::Code{0, {fizzy::Instr::i32_const, fizzy::Instr::end}, {0x42, 0, 0x42, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x420042);
}

TEST(execute, i64_const)
{
    fizzy::Module module;
    module.codesec.emplace_back(fizzy::Code{
        0, {fizzy::Instr::i64_const, fizzy::Instr::end}, {0x42, 0, 0x42, 0, 0, 0, 0, 1}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x0100000000420042ULL);
}

TEST(execute, i32_eqz)
{
    auto result = execute_unary_operation(fizzy::Instr::i32_eqz, 0);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_unary_operation(fizzy::Instr::i32_eqz, 1);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute, i32_eq)
{
    auto result = execute_binary_operation(fizzy::Instr::i32_eq, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(fizzy::Instr::i32_eq, 22, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute, i32_ne)
{
    auto result = execute_binary_operation(fizzy::Instr::i32_ne, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(fizzy::Instr::i32_ne, 22, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute, i32_clz)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i32_clz, 0x7f);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 32 - 7);
}

TEST(execute, i32_clz0)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i32_clz, 0);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 32);
}

TEST(execute, i32_ctz)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i32_ctz, 0x80);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 7);
}

TEST(execute, i32_ctz0)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i32_ctz, 0);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 32);
}

TEST(execute, i32_popcnt)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i32_popcnt, 0x7fff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 7 + 8);
}

TEST(execute, i32_add)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_add, 22, 20);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_sub)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_sub, 424242, 424200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_mul)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_mul, 2, 21);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_div_s)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_div_s, uint64_t(-84), 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], -42);
}

TEST(execute, i32_div_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_div_s, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i32_div_s_overflow)
{
    const auto [trap, ret] = execute_binary_operation(
        fizzy::Instr::i32_div_s, uint64_t(std::numeric_limits<int32_t>::min()), uint64_t(-1));

    ASSERT_TRUE(trap);
}

TEST(execute, i32_div_u)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_div_u, 84, 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_div_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_div_u, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i32_rem_s)
{
    const auto [trap, ret] =
        execute_binary_operation(fizzy::Instr::i32_rem_s, uint64_t(-4242), 4200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], -42);
}

TEST(execute, i32_rem_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_rem_s, uint64_t(-4242), 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i32_rem_u)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_rem_u, 4242, 4200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_rem_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_rem_u, 4242, 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i32_and)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_and, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00);
}

TEST(execute, i32_or)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_or, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffff);
}

TEST(execute, i32_xor)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_xor, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00ff);
}

TEST(execute, i32_shl)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_shl, 21, 1);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_shr_s)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_shr_s, uint64_t(-84), 1);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], -42);
}

TEST(execute, i32_shr_u)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_shr_u, 84, 1);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i32_rotl)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_rotl, 0xff000000, 4);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xf000000f);
}

TEST(execute, i32_rotr)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i32_rotr, 0x000000ff, 4);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xf000000f);
}

TEST(execute, i32_wrap_i64)
{
    const auto [trap, ret] =
        execute_unary_operation(fizzy::Instr::i32_wrap_i64, 0xffffffffffffffff);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffffff);
}

TEST(execute, i64_extend_i32_s_all_bits_set)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i64_extend_i32_s, 0xffffffff);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffffffffffffff);
}

TEST(execute, i64_extend_i32_s_one_bit_set)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i64_extend_i32_s, 0x80000000);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffffff80000000);
}

TEST(execute, i64_extend_i32_s_0)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i64_extend_i32_s, 0);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0);
}

TEST(execute, i64_extend_i32_s_1)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i64_extend_i32_s, 0x01);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x01);
}

TEST(execute, i64_extend_i32_u)
{
    const auto [trap, ret] = execute_unary_operation(fizzy::Instr::i64_extend_i32_u, 0xff000000);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x00000000ff000000);
}

TEST(execute, i64_add)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_add, 22, 20);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i64_sub)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_sub, 424242, 424200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i64_mul)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_mul, 2, 21);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i64_div_s)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_div_s, uint64_t(-84), 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], -42);
}

TEST(execute, i64_div_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_div_s, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i64_div_s_overflow)
{
    const auto [trap, ret] = execute_binary_operation(
        fizzy::Instr::i64_div_s, uint64_t(std::numeric_limits<int64_t>::min()), uint64_t(-1));

    ASSERT_TRUE(trap);
}

TEST(execute, i64_div_u)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_div_u, 84, 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i64_div_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_div_u, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i64_rem_s)
{
    const auto [trap, ret] =
        execute_binary_operation(fizzy::Instr::i64_rem_s, uint64_t(-4242), 4200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], -42);
}

TEST(execute, i64_rem_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_rem_s, uint64_t(-4242), 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i64_rem_u)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_rem_u, 4242, 4200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i64_rem_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_rem_u, 4242, 0);

    ASSERT_TRUE(trap);
}

TEST(execute, i64_and)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_and, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00);
}

TEST(execute, i64_or)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_or, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffff);
}

TEST(execute, i64_xor)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_xor, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00ff);
}

TEST(execute, i64_shl)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_shl, 21, 1);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i64_shr_s)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_shr_s, uint64_t(-84), 1);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], -42);
}

TEST(execute, i64_shr_u)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_shr_u, 84, 1);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, i64_rotl)
{
    const auto [trap, ret] =
        execute_binary_operation(fizzy::Instr::i64_rotl, 0xff00000000000000, 4);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xf00000000000000f);
}

TEST(execute, i64_rotr)
{
    const auto [trap, ret] = execute_binary_operation(fizzy::Instr::i64_rotr, 0xff, 4);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xf00000000000000f);
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
