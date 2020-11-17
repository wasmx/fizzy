// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

namespace
{
ExecutionResult execute_unary_operation(Instr instr, uint64_t arg)
{
    auto module{std::make_unique<Module>()};
    // type is currently needed only to get arity of function, so exact value types don't matter
    module->typesec.emplace_back(FuncType{{ValType::i32}, {ValType::i32}});
    module->funcsec.emplace_back(TypeIdx{0});
    module->codesec.emplace_back(Code{1, 0,
        {static_cast<uint8_t>(Instr::local_get), 0, 0, 0, 0, static_cast<uint8_t>(instr),
            static_cast<uint8_t>(Instr::end)}});

    return execute(*instantiate(std::move(module)), 0, {arg});
}

ExecutionResult execute_binary_operation(Instr instr, uint64_t lhs, uint64_t rhs)
{
    auto module{std::make_unique<Module>()};
    // type is currently needed only to get arity of function, so exact value types don't matter
    module->typesec.emplace_back(FuncType{{ValType::i32, ValType::i32}, {ValType::i32}});
    module->funcsec.emplace_back(TypeIdx{0});
    module->codesec.emplace_back(Code{2, 0,
        {static_cast<uint8_t>(Instr::local_get), 0, 0, 0, 0, static_cast<uint8_t>(Instr::local_get),
            1, 0, 0, 0, static_cast<uint8_t>(instr), static_cast<uint8_t>(Instr::end)}});

    return execute(*instantiate(std::move(module)), 0, {lhs, rhs});
}
}  // namespace

TEST(execute_numeric, i32_const)
{
    /* wat2wasm
    (func (result i32) (i32.const 0x420042))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0901070041c28088020b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0x420042));
}

TEST(execute_numeric, i64_const)
{
    /* wat2wasm
    (func (result i64) (i64.const 0x0100000000420042))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017e030201000a0e010c0042c280888280808080010b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0x0100000000420042));
}

TEST(execute_numeric, i32_eqz)
{
    EXPECT_THAT(execute_unary_operation(Instr::i32_eqz, 0), Result(1));
    EXPECT_THAT(execute_unary_operation(Instr::i32_eqz, 1), Result(0));
}

TEST(execute_numeric, i32_eq)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_eq, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_eq, 22, 22), Result(1));
}

TEST(execute_numeric, i32_ne)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_ne, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_ne, 22, 22), Result(0));
}

TEST(execute_numeric, i32_lt_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_lt_s, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_lt_s, 20, 22), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_lt_s, uint64_t(-41), uint64_t(-42)), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_lt_s, uint64_t(-42), uint64_t(-41)), Result(1));
}

TEST(execute_numeric, i32_lt_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_lt_u, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_lt_u, 20, 22), Result(1));
}

TEST(execute_numeric, i32_gt_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_gt_s, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_gt_s, 20, 22), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_gt_s, uint64_t(-41), uint64_t(-42)), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_gt_s, uint64_t(-42), uint64_t(-41)), Result(0));
}

TEST(execute_numeric, i32_gt_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_gt_u, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_gt_u, 20, 22), Result(0));
}

TEST(execute_numeric, i32_le_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_le_s, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_le_s, 20, 22), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_le_s, 20, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_le_s, uint64_t(-41), uint64_t(-42)), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_le_s, uint64_t(-42), uint64_t(-41)), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_le_s, uint64_t(-42), uint64_t(-42)), Result(1));
}

TEST(execute_numeric, i32_le_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_le_u, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_le_u, 20, 22), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_le_u, 20, 20), Result(1));
}

TEST(execute_numeric, i32_ge_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_ge_s, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_ge_s, 20, 22), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_ge_s, 20, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_ge_s, uint64_t(-41), uint64_t(-42)), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_ge_s, uint64_t(-42), uint64_t(-41)), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_ge_s, uint64_t(-42), uint64_t(-42)), Result(1));
}

TEST(execute_numeric, i32_ge_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_ge_u, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_ge_u, 20, 22), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_ge_u, 20, 20), Result(1));
}

TEST(execute_numeric, i64_eqz)
{
    EXPECT_THAT(execute_unary_operation(Instr::i64_eqz, 0), Result(1));
    EXPECT_THAT(execute_unary_operation(Instr::i64_eqz, 1), Result(0));
    // 64-bit value on the stack
    EXPECT_THAT(execute_unary_operation(fizzy::Instr::i64_eqz, 0xff00000000), Result(0));
    EXPECT_THAT(execute_unary_operation(fizzy::Instr::i64_eqz, 0xff00000001), Result(0));
}

TEST(execute_numeric, i64_eq)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_eq, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_eq, 22, 22), Result(1));
}

TEST(execute_numeric, i64_ne)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_ne, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_ne, 22, 22), Result(0));
}

TEST(execute_numeric, i64_lt_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_lt_s, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_lt_s, 20, 22), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_lt_s, uint64_t(-41), uint64_t(-42)), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_lt_s, uint64_t(-42), uint64_t(-41)), Result(1));
}

TEST(execute_numeric, i64_lt_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_lt_u, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_lt_u, 20, 22), Result(1));
}

TEST(execute_numeric, i64_gt_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_gt_s, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_gt_s, 20, 22), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_gt_s, uint64_t(-41), uint64_t(-42)), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_gt_s, uint64_t(-42), uint64_t(-41)), Result(0));
}

TEST(execute_numeric, i64_gt_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_gt_u, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_gt_u, 20, 22), Result(0));
}

TEST(execute_numeric, i64_le_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_le_s, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_le_s, 20, 22), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_le_s, 20, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_le_s, uint64_t(-41), uint64_t(-42)), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_le_s, uint64_t(-42), uint64_t(-41)), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_le_s, uint64_t(-42), uint64_t(-42)), Result(1));
}

TEST(execute_numeric, i64_le_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_le_u, 22, 20), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_le_u, 20, 22), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_le_u, 20, 20), Result(1));
}

TEST(execute_numeric, i64_ge_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_ge_s, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_ge_s, 20, 22), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_ge_s, 20, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_ge_s, uint64_t(-41), uint64_t(-42)), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_ge_s, uint64_t(-42), uint64_t(-41)), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_ge_s, uint64_t(-42), uint64_t(-42)), Result(1));
}

TEST(execute_numeric, i64_ge_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_ge_u, 22, 20), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i64_ge_u, 20, 22), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i64_ge_u, 20, 20), Result(1));
}

TEST(execute_numeric, i32_clz)
{
    EXPECT_THAT(execute_unary_operation(Instr::i32_clz, 0x7f), Result(32 - 7));
}

TEST(execute_numeric, i32_clz0)
{
    EXPECT_THAT(execute_unary_operation(Instr::i32_clz, 0), Result(32));
}

TEST(execute_numeric, i32_ctz)
{
    EXPECT_THAT(execute_unary_operation(Instr::i32_ctz, 0x80), Result(7));
}

TEST(execute_numeric, i32_ctz0)
{
    EXPECT_THAT(execute_unary_operation(Instr::i32_ctz, 0), Result(32));
}

TEST(execute_numeric, i32_popcnt)
{
    EXPECT_THAT(execute_unary_operation(Instr::i32_popcnt, 0x7fff00), Result(7 + 8));
}

TEST(execute_numeric, i32_add)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_add, 22, 20), Result(42));
}

TEST(execute_numeric, i32_sub)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_sub, 424242, 424200), Result(42));
}

TEST(execute_numeric, i32_mul)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_mul, 2, 21), Result(42));
}

TEST(execute_numeric, i32_div_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_div_s, uint64_t(-84), 2), Result(-42));
}

TEST(execute_numeric, i32_div_s_by_zero)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_div_s, 84, 0), Traps());
}

TEST(execute_numeric, i32_div_s_overflow)
{
    EXPECT_THAT(execute_binary_operation(
                    Instr::i32_div_s, uint64_t(std::numeric_limits<int32_t>::min()), uint64_t(-1)),
        Traps());
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

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0xffffffff));
}

TEST(execute_numeric, i32_div_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_div_u, 84, 2), Result(42));
}

TEST(execute_numeric, i32_div_u_by_zero)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_div_u, 84, 0), Traps());
}

TEST(execute_numeric, i32_rem_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_rem_s, uint64_t(-4242), 4200), Result(-42));
    constexpr auto i32_min = std::numeric_limits<int32_t>::min();
    EXPECT_THAT(
        execute_binary_operation(Instr::i32_rem_s, uint64_t(i32_min), uint64_t(-1)), Result(0));
}

TEST(execute_numeric, i32_rem_s_by_zero)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_rem_s, uint64_t(-4242), 0), Traps());
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

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0xffffffff));
}

TEST(execute_numeric, i32_rem_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_rem_u, 4242, 4200), Result(42));
}

TEST(execute_numeric, i32_rem_u_by_zero)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_rem_u, 4242, 0), Traps());
}

TEST(execute_numeric, i32_and)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_and, 0x00ffff, 0xffff00), Result(0xff00));
}

TEST(execute_numeric, i32_or)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_or, 0x00ffff, 0xffff00), Result(0xffffff));
}

TEST(execute_numeric, i32_xor)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_xor, 0x00ffff, 0xffff00), Result(0xff00ff));
}

TEST(execute_numeric, i32_shl)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_shl, 21, 1), Result(42));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 0), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 1), Result(0xfffffffe));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 31), Result(0x80000000));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 32), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 33), Result(0xfffffffe));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shl, 0xffffffff, 63), Result(0x80000000));
}

TEST(execute_numeric, i32_shr_s)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, uint64_t(-84), 1), Result(-42));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 0), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 1), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 31), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 32), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 33), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0xffffffff, 63), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 0), Result(0x7fffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 1), Result(0x3fffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 30), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 31), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 32), Result(0x7fffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 33), Result(0x3fffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 62), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 0x7fffffff, 63), Result(0));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_s, 1, uint32_t(-1)), Result(0));
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

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0xffffffff));
}

TEST(execute_numeric, i32_shr_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_u, 84, 1), Result(42));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 0), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 1), Result(0x7fffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 31), Result(1));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 32), Result(0xffffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 33), Result(0x7fffffff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_shr_u, 0xffffffff, 63), Result(1));
}

TEST(execute_numeric, i32_rotl)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 0), Result(0xff000000));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 1), Result(0xfe000001));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 31), Result(0x7f800000));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 32), Result(0xff000000));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 33), Result(0xfe000001));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotl, 0xff000000, 63), Result(0x7f800000));
}

TEST(execute_numeric, i32_rotr)
{
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 0), Result(0x000000ff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 1), Result(0x8000007f));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 31), Result(0x000001fe));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 32), Result(0x000000ff));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 33), Result(0x8000007f));
    EXPECT_THAT(execute_binary_operation(Instr::i32_rotr, 0x000000ff, 63), Result(0x000001fe));
}

TEST(execute_numeric, i32_wrap_i64)
{
    // <=32-bits set
    EXPECT_THAT(execute_unary_operation(Instr::i32_wrap_i64, 0xffffffff), Result(0xffffffff));
    // >32-bits set
    EXPECT_THAT(
        execute_unary_operation(Instr::i32_wrap_i64, 0xffffffffffffffff), Result(0xffffffff));
}

TEST(execute_numeric, i64_extend_i32_s_all_bits_set)
{
    EXPECT_THAT(
        execute_unary_operation(Instr::i64_extend_i32_s, 0xffffffff), Result(0xffffffffffffffff));
}

TEST(execute_numeric, i64_extend_i32_s_one_bit_set)
{
    EXPECT_THAT(
        execute_unary_operation(Instr::i64_extend_i32_s, 0x80000000), Result(0xffffffff80000000));
}

TEST(execute_numeric, i64_extend_i32_s_0)
{
    EXPECT_THAT(execute_unary_operation(Instr::i64_extend_i32_s, 0), Result(0));
}

TEST(execute_numeric, i64_extend_i32_s_1)
{
    EXPECT_THAT(execute_unary_operation(Instr::i64_extend_i32_s, 0x01), Result(0x01));
}

TEST(execute_numeric, i64_extend_i32_u)
{
    EXPECT_THAT(
        execute_unary_operation(Instr::i64_extend_i32_u, 0xff000000), Result(0x00000000ff000000));
}

TEST(execute_numeric, i64_extend_i32_u_2)
{
    /* wat2wasm
    (func (param i32) (result i64)
      i64.const 0xdeadbeefdeadbeef
      drop
      local.get 0
      i64.extend_i32_u
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017e030201000a1201100042effdb6f5fdddefd65e1a2000ad0b");

    auto instance = instantiate(parse(wasm));
    const auto r = execute(*instance, 0, {0xff000000});
    EXPECT_THAT(r, Result(uint64_t{0x00000000ff000000}));
}

TEST(execute_numeric, i64_clz)
{
    EXPECT_THAT(execute_unary_operation(Instr::i64_clz, 0x7f), Result(64 - 7));
}

TEST(execute_numeric, i64_clz0)
{
    EXPECT_THAT(execute_unary_operation(Instr::i64_clz, 0), Result(64));
}

TEST(execute_numeric, i64_ctz)
{
    EXPECT_THAT(execute_unary_operation(Instr::i64_ctz, 0x80), Result(7));
}

TEST(execute_numeric, i64_ctz0)
{
    EXPECT_THAT(execute_unary_operation(Instr::i64_ctz, 0), Result(64));
}

TEST(execute_numeric, i64_popcnt)
{
    EXPECT_THAT(execute_unary_operation(Instr::i64_popcnt, 0x7fff00), Result(7 + 8));
}

TEST(execute_numeric, i64_add)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_add, 22, 20), Result(42));
}

TEST(execute_numeric, i64_sub)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_sub, 424242, 424200), Result(42));
}

TEST(execute_numeric, i64_mul)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_mul, 2, 21), Result(42));
}

TEST(execute_numeric, i64_div_s)
{
    EXPECT_THAT(
        execute_binary_operation(Instr::i64_div_s, uint64_t(-84), 2), Result(uint64_t(-42)));
}

TEST(execute_numeric, i64_div_s_by_zero)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_div_s, 84, 0), Traps());
}

TEST(execute_numeric, i64_div_s_overflow)
{
    EXPECT_THAT(execute_binary_operation(
                    Instr::i64_div_s, uint64_t(std::numeric_limits<int64_t>::min()), uint64_t(-1)),
        Traps());
}

TEST(execute_numeric, i64_div_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_div_u, 84, 2), Result(42));
}

TEST(execute_numeric, i64_div_u_by_zero)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_div_u, 84, 0), Traps());
}

TEST(execute_numeric, i64_rem_s)
{
    EXPECT_THAT(
        execute_binary_operation(Instr::i64_rem_s, uint64_t(-4242), 4200), Result(uint64_t(-42)));
    constexpr auto i64_min = std::numeric_limits<int64_t>::min();
    EXPECT_THAT(
        execute_binary_operation(Instr::i64_rem_s, uint64_t(i64_min), uint64_t(-1)), Result(0));
}

TEST(execute_numeric, i64_rem_s_by_zero)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_rem_s, uint64_t(-4242), 0), Traps());
}

TEST(execute_numeric, i64_rem_u)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_rem_u, 4242, 4200), Result(42));
}

TEST(execute_numeric, i64_rem_u_by_zero)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_rem_u, 4242, 0), Traps());
}

TEST(execute_numeric, i64_and)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_and, 0x00ffff, 0xffff00), Result(0xff00));
}

TEST(execute_numeric, i64_or)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_or, 0x00ffff, 0xffff00), Result(0xffffff));
}

TEST(execute_numeric, i64_xor)
{
    EXPECT_THAT(execute_binary_operation(Instr::i64_xor, 0x00ffff, 0xffff00), Result(0xff00ff));
}

TEST(execute_numeric, i64_shl)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_THAT(ebo(Instr::i64_shl, 21, 1), Result(42));
    EXPECT_THAT(ebo(Instr::i64_shl, 0xffffffffffffffff, 0), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shl, 0xffffffffffffffff, 1), Result(0xfffffffffffffffe));
    EXPECT_THAT(ebo(Instr::i64_shl, 0xffffffffffffffff, 63), Result(0x8000000000000000));
    EXPECT_THAT(ebo(Instr::i64_shl, 0xffffffffffffffff, 64), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shl, 0xffffffffffffffff, 65), Result(0xfffffffffffffffe));
    EXPECT_THAT(ebo(Instr::i64_shl, 0xffffffffffffffff, 127), Result(0x8000000000000000));
}

TEST(execute_numeric, i64_shr_s)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_THAT(ebo(Instr::i64_shr_s, uint64_t(-84), 1), Result(uint64_t(-42)));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 0), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 1), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 63), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 64), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 65), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0xffffffffffffffff, 127), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 0), Result(0x7fffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 1), Result(0x3fffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 62), Result(1));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 63), Result(0));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 64), Result(0x7fffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 65), Result(0x3fffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 126), Result(1));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 0x7fffffffffffffff, 127), Result(0));
    EXPECT_THAT(ebo(Instr::i64_shr_s, 1, uint64_t(-1)), Result(0));
}

TEST(execute_numeric, i64_shr_u)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_THAT(ebo(Instr::i64_shr_u, 84, 1), Result(42));
    EXPECT_THAT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 0), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 1), Result(0x7fffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 63), Result(1));
    EXPECT_THAT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 64), Result(0xffffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 65), Result(0x7fffffffffffffff));
    EXPECT_THAT(ebo(Instr::i64_shr_u, 0xffffffffffffffff, 127), Result(1));
}

TEST(execute_numeric, i64_rotl)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_THAT(ebo(Instr::i64_rotl, 0xff00000000000000, 0), Result(0xff00000000000000));
    EXPECT_THAT(ebo(Instr::i64_rotl, 0xff00000000000000, 1), Result(0xfe00000000000001));
    EXPECT_THAT(ebo(Instr::i64_rotl, 0xff00000000000000, 63), Result(0x7f80000000000000));
    EXPECT_THAT(ebo(Instr::i64_rotl, 0xff00000000000000, 64), Result(0xff00000000000000));
    EXPECT_THAT(ebo(Instr::i64_rotl, 0xff00000000000000, 65), Result(0xfe00000000000001));
    EXPECT_THAT(ebo(Instr::i64_rotl, 0xff00000000000000, 127), Result(0x7f80000000000000));
}

TEST(execute_numeric, i64_rotr)
{
    constexpr auto ebo = execute_binary_operation;
    EXPECT_THAT(ebo(Instr::i64_rotr, 0x00000000000000ff, 0), Result(0x00000000000000ff));
    EXPECT_THAT(ebo(Instr::i64_rotr, 0x00000000000000ff, 1), Result(0x800000000000007f));
    EXPECT_THAT(ebo(Instr::i64_rotr, 0x00000000000000ff, 63), Result(0x00000000000001fe));
    EXPECT_THAT(ebo(Instr::i64_rotr, 0x00000000000000ff, 64), Result(0x00000000000000ff));
    EXPECT_THAT(ebo(Instr::i64_rotr, 0x00000000000000ff, 65), Result(0x800000000000007f));
    EXPECT_THAT(ebo(Instr::i64_rotr, 0x00000000000000ff, 127), Result(0x00000000000001fe));
}
