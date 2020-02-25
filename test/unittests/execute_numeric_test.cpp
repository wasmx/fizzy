#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;

namespace
{
execution_result execute_unary_operation(Instr instr, uint64_t arg)
{
    Module module;
    module.codesec.emplace_back(Code{0, {Instr::local_get, instr, Instr::end}, {0, 0, 0, 0}});

    return execute(module, 0, {arg});
}

execution_result execute_binary_operation(Instr instr, uint64_t lhs, uint64_t rhs)
{
    Module module;
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, instr, Instr::end}, {0, 0, 0, 0, 1, 0, 0, 0}});

    return execute(module, 0, {lhs, rhs});
}
}  // namespace

TEST(execute_numeric, i32_const)
{
    Module module;
    module.codesec.emplace_back(Code{0, {Instr::i32_const, Instr::end}, {0x42, 0, 0x42, 0}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x420042);
}

TEST(execute_numeric, i64_const)
{
    Module module;
    module.codesec.emplace_back(
        Code{0, {Instr::i64_const, Instr::end}, {0x42, 0, 0x42, 0, 0, 0, 0, 1}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x0100000000420042ULL);
}

TEST(execute_numeric, i32_eqz)
{
    auto result = execute_unary_operation(Instr::i32_eqz, 0);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_unary_operation(Instr::i32_eqz, 1);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    // Dirty stack
    result = execute_unary_operation(fizzy::Instr::i32_eqz, 0xff00000000);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_unary_operation(fizzy::Instr::i32_eqz, 0xff00000001);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute_numeric, i32_eq)
{
    auto result = execute_binary_operation(Instr::i32_eq, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_eq, 22, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i32_ne)
{
    auto result = execute_binary_operation(Instr::i32_ne, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_ne, 22, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute_numeric, i32_lt_s)
{
    auto result = execute_binary_operation(Instr::i32_lt_s, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_lt_s, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_lt_s, uint64_t(-41), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_lt_s, uint64_t(-42), uint64_t(-41));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i32_lt_u)
{
    auto result = execute_binary_operation(Instr::i32_lt_u, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_lt_u, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i32_gt_s)
{
    auto result = execute_binary_operation(Instr::i32_gt_s, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_gt_s, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_gt_s, uint64_t(-41), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_gt_s, uint64_t(-42), uint64_t(-41));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute_numeric, i32_gt_u)
{
    auto result = execute_binary_operation(Instr::i32_gt_u, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_gt_u, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute_numeric, i32_le_s)
{
    auto result = execute_binary_operation(Instr::i32_le_s, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_le_s, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_le_s, 20, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_le_s, uint64_t(-41), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_le_s, uint64_t(-42), uint64_t(-41));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_le_s, uint64_t(-42), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i32_le_u)
{
    auto result = execute_binary_operation(Instr::i32_le_u, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_le_u, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_le_u, 20, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i32_ge_s)
{
    auto result = execute_binary_operation(Instr::i32_ge_s, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_ge_s, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_ge_s, 20, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_ge_s, uint64_t(-41), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_ge_s, uint64_t(-42), uint64_t(-41));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_ge_s, uint64_t(-42), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i32_ge_u)
{
    auto result = execute_binary_operation(Instr::i32_ge_u, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i32_ge_u, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i32_ge_u, 20, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i64_eqz)
{
    auto result = execute_unary_operation(Instr::i64_eqz, 0);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_unary_operation(Instr::i64_eqz, 1);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    // 64-bit value on the stack
    result = execute_unary_operation(fizzy::Instr::i64_eqz, 0xff00000000);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_unary_operation(fizzy::Instr::i64_eqz, 0xff00000001);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute_numeric, i64_eq)
{
    auto result = execute_binary_operation(Instr::i64_eq, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_eq, 22, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i64_ne)
{
    auto result = execute_binary_operation(Instr::i64_ne, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_ne, 22, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute_numeric, i64_lt_s)
{
    auto result = execute_binary_operation(Instr::i64_lt_s, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_lt_s, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_lt_s, uint64_t(-41), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_lt_s, uint64_t(-42), uint64_t(-41));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i64_lt_u)
{
    auto result = execute_binary_operation(Instr::i64_lt_u, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_lt_u, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i64_gt_s)
{
    auto result = execute_binary_operation(Instr::i64_gt_s, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_gt_s, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_gt_s, uint64_t(-41), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_gt_s, uint64_t(-42), uint64_t(-41));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute_numeric, i64_gt_u)
{
    auto result = execute_binary_operation(Instr::i64_gt_u, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_gt_u, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);
}

TEST(execute_numeric, i64_le_s)
{
    auto result = execute_binary_operation(Instr::i64_le_s, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_le_s, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_le_s, 20, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_le_s, uint64_t(-41), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_le_s, uint64_t(-42), uint64_t(-41));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_le_s, uint64_t(-42), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i64_le_u)
{
    auto result = execute_binary_operation(Instr::i64_le_u, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_le_u, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_le_u, 20, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i64_ge_s)
{
    auto result = execute_binary_operation(Instr::i64_ge_s, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_ge_s, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_ge_s, 20, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_ge_s, uint64_t(-41), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_ge_s, uint64_t(-42), uint64_t(-41));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_ge_s, uint64_t(-42), uint64_t(-42));

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i64_ge_u)
{
    auto result = execute_binary_operation(Instr::i64_ge_u, 22, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute_binary_operation(Instr::i64_ge_u, 20, 22);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 0);

    result = execute_binary_operation(Instr::i64_ge_u, 20, 20);

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);
}

TEST(execute_numeric, i32_clz)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i32_clz, 0x7f);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 32 - 7);
}

TEST(execute_numeric, i32_clz0)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i32_clz, 0);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 32);
}

TEST(execute_numeric, i32_ctz)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i32_ctz, 0x80);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 7);
}

TEST(execute_numeric, i32_ctz0)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i32_ctz, 0);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 32);
}

TEST(execute_numeric, i32_popcnt)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i32_popcnt, 0x7fff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 7 + 8);
}

TEST(execute_numeric, i32_add)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_add, 22, 20);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i32_sub)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_sub, 424242, 424200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i32_mul)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_mul, 2, 21);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i32_div_s)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_div_s, uint64_t(-84), 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], uint32_t(-42));
}

TEST(execute_numeric, i32_div_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_div_s, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i32_div_s_overflow)
{
    const auto [trap, ret] = execute_binary_operation(
        Instr::i32_div_s, uint64_t(std::numeric_limits<int32_t>::min()), uint64_t(-1));

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i32_div_s_stack_value)
{
    /* wat2wasm
    (func (result i64)
      (i32.div_s (i32.const -3) (i32.const 2))  ;; Should put 0xffffffff on the stack.
      i64.extend_u/i32
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017e030201000a0a010800417d41026dad0b");

    EXPECT_RESULT(execute(parse(wasm), 0, {}), 0xffffffff);
}

TEST(execute_numeric, i32_div_u)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_div_u, 84, 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i32_div_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_div_u, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i32_rem_s)
{
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rem_s, uint64_t(-4242), 4200), uint32_t(-42));
    constexpr auto i32_min = std::numeric_limits<int32_t>::min();
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rem_s, uint64_t(i32_min), uint64_t(-1)), 0);
}

TEST(execute_numeric, i32_rem_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_rem_s, uint64_t(-4242), 0);

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i32_rem_s_stack_value)
{
    /* wat2wasm
    (func (result i64)
      (i32.rem_s (i32.const -3) (i32.const 2))  ;; Should put 0xffffffff on the stack.
      i64.extend_u/i32
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017e030201000a0a010800417d41026fad0b");

    EXPECT_RESULT(execute(parse(wasm), 0, {}), 0xffffffff);
}

TEST(execute_numeric, i32_rem_u)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_rem_u, 4242, 4200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i32_rem_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_rem_u, 4242, 0);

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i32_and)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_and, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00);
}

TEST(execute_numeric, i32_or)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_or, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffff);
}

TEST(execute_numeric, i32_xor)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i32_xor, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00ff);
}

TEST(execute_numeric, i32_shl)
{
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shl, 21, 1), 42);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 0), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 1), 0xfffffffe);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 31), 0x80000000);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 32), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 33), 0xfffffffe);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 63), 0x80000000);
}

TEST(execute_numeric, i32_shr_s)
{
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, uint64_t(-84), 1), uint32_t(-42));
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 0), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 1), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 31), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 32), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 33), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 63), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 0), 0x7fffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 1), 0x3fffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 30), 1);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 31), 0);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 32), 0x7fffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 33), 0x3fffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 62), 1);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 63), 0);
}

TEST(execute_numeric, i32_shr_s_stack_value)
{
    /* wat2wasm
    (func (result i64)
      i32.const -1
      i32.const 0
      i32.shr_s         ;; Must put 0xffffffff on the stack.
      i64.extend_u/i32
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017e030201000a0a010800417f410075ad0b");

    EXPECT_RESULT(execute(parse(wasm), 0, {}), 0xffffffff);
}

TEST(execute_numeric, i32_shr_u)
{
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_u, 84, 1), 42);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 0), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 1), 0x7fffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 31), 1);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 32), 0xffffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 33), 0x7fffffff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 63), 1);
}

TEST(execute_numeric, i32_rotl)
{
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 0), 0xff000000);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 1), 0xfe000001);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 31), 0x7f800000);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 32), 0xff000000);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 33), 0xfe000001);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 63), 0x7f800000);
}

TEST(execute_numeric, i32_rotr)
{
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 0), 0x000000ff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 1), 0x8000007f);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 31), 0x000001fe);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 32), 0x000000ff);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 33), 0x8000007f);
    EXPECT_RESULT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 63), 0x000001fe);
}

TEST(execute_numeric, i32_wrap_i64)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i32_wrap_i64, 0xffffffffffffffff);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffffff);
}

TEST(execute_numeric, i64_extend_i32_s_all_bits_set)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_extend_i32_s, 0xffffffff);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffffffffffffff);
}

TEST(execute_numeric, i64_extend_i32_s_one_bit_set)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_extend_i32_s, 0x80000000);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffffff80000000);
}

TEST(execute_numeric, i64_extend_i32_s_0)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_extend_i32_s, 0);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0);
}

TEST(execute_numeric, i64_extend_i32_s_1)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_extend_i32_s, 0x01);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x01);
}

TEST(execute_numeric, i64_extend_i32_u)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_extend_i32_u, 0xff000000);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x00000000ff000000);
}

TEST(execute_numeric, i64_clz)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_clz, 0x7f);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 64 - 7);
}

TEST(execute_numeric, i64_clz0)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_clz, 0);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 64);
}

TEST(execute_numeric, i64_ctz)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_ctz, 0x80);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 7);
}

TEST(execute_numeric, i64_ctz0)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_ctz, 0);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 64);
}

TEST(execute_numeric, i64_popcnt)
{
    const auto [trap, ret] = execute_unary_operation(Instr::i64_popcnt, 0x7fff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 7 + 8);
}

TEST(execute_numeric, i64_add)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_add, 22, 20);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i64_sub)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_sub, 424242, 424200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i64_mul)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_mul, 2, 21);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i64_div_s)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_div_s, uint64_t(-84), 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], uint64_t(-42));
}

TEST(execute_numeric, i64_div_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_div_s, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i64_div_s_overflow)
{
    const auto [trap, ret] = execute_binary_operation(
        Instr::i64_div_s, uint64_t(std::numeric_limits<int64_t>::min()), uint64_t(-1));

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i64_div_u)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_div_u, 84, 2);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i64_div_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_div_u, 84, 0);

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i64_rem_s)
{
    EXPECT_RESULT(execute_binary_operation(Instr::i64_rem_s, uint64_t(-4242), 4200), uint64_t(-42));
    constexpr auto i64_min = std::numeric_limits<int64_t>::min();
    EXPECT_RESULT(execute_binary_operation(Instr::i64_rem_s, uint64_t(i64_min), uint64_t(-1)), 0);
}

TEST(execute_numeric, i64_rem_s_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_rem_s, uint64_t(-4242), 0);

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i64_rem_u)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_rem_u, 4242, 4200);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute_numeric, i64_rem_u_by_zero)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_rem_u, 4242, 0);

    ASSERT_TRUE(trap);
}

TEST(execute_numeric, i64_and)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_and, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00);
}

TEST(execute_numeric, i64_or)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_or, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xffffff);
}

TEST(execute_numeric, i64_xor)
{
    const auto [trap, ret] = execute_binary_operation(Instr::i64_xor, 0x00ffff, 0xffff00);

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0xff00ff);
}

TEST(execute_numeric, i64_shl)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_RESULT(ebo(Instr::i64_shl, 21, 1), 42);
    EXPECT_RESULT(ebo(Instr::i64_shl, 0xffffffffffffffff, 0), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shl, 0xffffffffffffffff, 1), 0xfffffffffffffffe);
    EXPECT_RESULT(ebo(Instr::i64_shl, 0xffffffffffffffff, 63), 0x8000000000000000);
    EXPECT_RESULT(ebo(Instr::i64_shl, 0xffffffffffffffff, 64), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shl, 0xffffffffffffffff, 65), 0xfffffffffffffffe);
    EXPECT_RESULT(ebo(Instr::i64_shl, 0xffffffffffffffff, 127), 0x8000000000000000);
}

TEST(execute_numeric, i64_shr_s)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_RESULT(ebo(Instr::i64_shr_s, uint64_t(-84), 1), uint64_t(-42));
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 0), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 1), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 63), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 64), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 65), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 127), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 0), 0x7fffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 1), 0x3fffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 62), 1);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 63), 0);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 64), 0x7fffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 65), 0x3fffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 126), 1);
    EXPECT_RESULT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 127), 0);
}

TEST(execute_numeric, i64_shr_u)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_RESULT(ebo(Instr::i64_shr_u, 84, 1), 42);
    EXPECT_RESULT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 0), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 1), 0x7fffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 63), 1);
    EXPECT_RESULT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 64), 0xffffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 65), 0x7fffffffffffffff);
    EXPECT_RESULT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 127), 1);
}

TEST(execute_numeric, i64_rotl)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_RESULT(ebo(Instr::i64_rotl, 0xff00000000000000, 0), 0xff00000000000000);
    EXPECT_RESULT(ebo(Instr::i64_rotl, 0xff00000000000000, 1), 0xfe00000000000001);
    EXPECT_RESULT(ebo(Instr::i64_rotl, 0xff00000000000000, 63), 0x7f80000000000000);
    EXPECT_RESULT(ebo(Instr::i64_rotl, 0xff00000000000000, 64), 0xff00000000000000);
    EXPECT_RESULT(ebo(Instr::i64_rotl, 0xff00000000000000, 65), 0xfe00000000000001);
    EXPECT_RESULT(ebo(Instr::i64_rotl, 0xff00000000000000, 127), 0x7f80000000000000);
}

TEST(execute_numeric, i64_rotr)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_RESULT(ebo(Instr::i64_rotr, 0x00000000000000ff, 0), 0x00000000000000ff);
    EXPECT_RESULT(ebo(Instr::i64_rotr, 0x00000000000000ff, 1), 0x800000000000007f);
    EXPECT_RESULT(ebo(Instr::i64_rotr, 0x00000000000000ff, 63), 0x00000000000001fe);
    EXPECT_RESULT(ebo(Instr::i64_rotr, 0x00000000000000ff, 64), 0x00000000000000ff);
    EXPECT_RESULT(ebo(Instr::i64_rotr, 0x00000000000000ff, 65), 0x800000000000007f);
    EXPECT_RESULT(ebo(Instr::i64_rotr, 0x00000000000000ff, 127), 0x00000000000001fe);
}
