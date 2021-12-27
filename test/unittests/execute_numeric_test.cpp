// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "bitcount_test_cases.hpp"
#include "execute.hpp"
#include "instructions.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>
#include <test/utils/typed_value.hpp>

using namespace fizzy;
using namespace fizzy::test;

namespace
{
auto create_unary_operation_executor(Instr instr)
{
    const auto& instr_type = get_instruction_type_table()[static_cast<uint8_t>(instr)];
    EXPECT_EQ(instr_type.inputs.size(), 1);
    EXPECT_EQ(instr_type.outputs.size(), 1);

    auto module{std::make_unique<Module>()};
    module->typesec.emplace_back(FuncType{{instr_type.inputs[0]}, {instr_type.outputs[0]}});
    module->funcsec.emplace_back(TypeIdx{0});
    module->codesec.emplace_back(Code{1, 0,
        {static_cast<uint8_t>(Instr::local_get), 0, 0, 0, 0, static_cast<uint8_t>(instr),
            static_cast<uint8_t>(Instr::end)}});

    auto instance = instantiate(std::move(module));

    return
        [instance = std::move(instance)](TypedValue arg) { return execute(*instance, 0, {arg}); };
}

auto create_binary_operation_executor(Instr instr)
{
    const auto& instr_type = get_instruction_type_table()[static_cast<uint8_t>(instr)];
    EXPECT_EQ(instr_type.inputs.size(), 2);
    EXPECT_EQ(instr_type.outputs.size(), 1);

    auto module{std::make_unique<Module>()};
    module->typesec.emplace_back(
        FuncType{{instr_type.inputs[0], instr_type.inputs[1]}, {instr_type.outputs[0]}});
    module->funcsec.emplace_back(TypeIdx{0});
    module->codesec.emplace_back(Code{2, 0,
        {static_cast<uint8_t>(Instr::local_get), 0, 0, 0, 0, static_cast<uint8_t>(Instr::local_get),
            1, 0, 0, 0, static_cast<uint8_t>(instr), static_cast<uint8_t>(Instr::end)}});

    auto instance = instantiate(std::move(module));

    return [instance = std::move(instance)](TypedValue lhs, TypedValue rhs) {
        return execute(*instance, 0, {lhs, rhs});
    };
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
    const auto i32_eqz = create_unary_operation_executor(Instr::i32_eqz);
    EXPECT_THAT(i32_eqz(0), Result(1));
    EXPECT_THAT(i32_eqz(1), Result(0));
}

TEST(execute_numeric, i32_eq)
{
    const auto i32_eq = create_binary_operation_executor(Instr::i32_eq);
    EXPECT_THAT(i32_eq(22, 20), Result(0));
    EXPECT_THAT(i32_eq(22, 22), Result(1));
}

TEST(execute_numeric, i32_ne)
{
    const auto i32_ne = create_binary_operation_executor(Instr::i32_ne);
    EXPECT_THAT(i32_ne(22, 20), Result(1));
    EXPECT_THAT(i32_ne(22, 22), Result(0));
}

TEST(execute_numeric, i32_lt_s)
{
    const auto i32_lt_s = create_binary_operation_executor(Instr::i32_lt_s);
    EXPECT_THAT(i32_lt_s(22, 20), Result(0));
    EXPECT_THAT(i32_lt_s(20, 22), Result(1));
    EXPECT_THAT(i32_lt_s(uint32_t(-41), uint32_t(-42)), Result(0));
    EXPECT_THAT(i32_lt_s(uint32_t(-42), uint32_t(-41)), Result(1));
}

TEST(execute_numeric, i32_lt_u)
{
    const auto i32_lt_u = create_binary_operation_executor(Instr::i32_lt_u);
    EXPECT_THAT(i32_lt_u(22, 20), Result(0));
    EXPECT_THAT(i32_lt_u(20, 22), Result(1));
}

TEST(execute_numeric, i32_gt_s)
{
    const auto i32_gt_s = create_binary_operation_executor(Instr::i32_gt_s);
    EXPECT_THAT(i32_gt_s(22, 20), Result(1));
    EXPECT_THAT(i32_gt_s(20, 22), Result(0));
    EXPECT_THAT(i32_gt_s(uint32_t(-41), uint32_t(-42)), Result(1));
    EXPECT_THAT(i32_gt_s(uint32_t(-42), uint32_t(-41)), Result(0));
}

TEST(execute_numeric, i32_gt_u)
{
    const auto i32_gt_u = create_binary_operation_executor(Instr::i32_gt_u);
    EXPECT_THAT(i32_gt_u(22, 20), Result(1));
    EXPECT_THAT(i32_gt_u(20, 22), Result(0));
}

TEST(execute_numeric, i32_le_s)
{
    const auto i32_le_s = create_binary_operation_executor(Instr::i32_le_s);
    EXPECT_THAT(i32_le_s(22, 20), Result(0));
    EXPECT_THAT(i32_le_s(20, 22), Result(1));
    EXPECT_THAT(i32_le_s(20, 20), Result(1));
    EXPECT_THAT(i32_le_s(uint32_t(-41), uint32_t(-42)), Result(0));
    EXPECT_THAT(i32_le_s(uint32_t(-42), uint32_t(-41)), Result(1));
    EXPECT_THAT(i32_le_s(uint32_t(-42), uint32_t(-42)), Result(1));
}

TEST(execute_numeric, i32_le_u)
{
    const auto i32_le_u = create_binary_operation_executor(Instr::i32_le_u);
    EXPECT_THAT(i32_le_u(22, 20), Result(0));
    EXPECT_THAT(i32_le_u(20, 22), Result(1));
    EXPECT_THAT(i32_le_u(20, 20), Result(1));
}

TEST(execute_numeric, i32_ge_s)
{
    const auto i32_ge_s = create_binary_operation_executor(Instr::i32_ge_s);
    EXPECT_THAT(i32_ge_s(22, 20), Result(1));
    EXPECT_THAT(i32_ge_s(20, 22), Result(0));
    EXPECT_THAT(i32_ge_s(20, 20), Result(1));
    EXPECT_THAT(i32_ge_s(uint32_t(-41), uint32_t(-42)), Result(1));
    EXPECT_THAT(i32_ge_s(uint32_t(-42), uint32_t(-41)), Result(0));
    EXPECT_THAT(i32_ge_s(uint32_t(-42), uint32_t(-42)), Result(1));
}

TEST(execute_numeric, i32_ge_u)
{
    const auto i32_ge_u = create_binary_operation_executor(Instr::i32_ge_u);
    EXPECT_THAT(i32_ge_u(22, 20), Result(1));
    EXPECT_THAT(i32_ge_u(20, 22), Result(0));
    EXPECT_THAT(i32_ge_u(20, 20), Result(1));
}

TEST(execute_numeric, i64_eqz)
{
    const auto i64_eqz = create_unary_operation_executor(Instr::i64_eqz);
    EXPECT_THAT(i64_eqz(0_u64), Result(1_u32));
    EXPECT_THAT(i64_eqz(1_u64), Result(0_u32));
    EXPECT_THAT(i64_eqz(0xff00000000_u64), Result(0_u32));
    EXPECT_THAT(i64_eqz(0xff00000001_u64), Result(0_u32));
    EXPECT_THAT(i64_eqz(0xffffffff00000000_u64), Result(0_u32));
    EXPECT_THAT(i64_eqz(0xffffffff00000001_u64), Result(0_u32));
    EXPECT_THAT(i64_eqz(0x8000000000000000_u64), Result(0_u32));
    EXPECT_THAT(i64_eqz(0x8000000000000001_u64), Result(0_u32));
}

TEST(execute_numeric, i64_eq)
{
    const auto i64_eq = create_binary_operation_executor(Instr::i64_eq);
    EXPECT_THAT(i64_eq(22_u64, 20_u64), Result(0));
    EXPECT_THAT(i64_eq(22_u64, 22_u64), Result(1));
}

TEST(execute_numeric, i64_ne)
{
    const auto i64_ne = create_binary_operation_executor(Instr::i64_ne);
    EXPECT_THAT(i64_ne(22_u64, 20_u64), Result(1));
    EXPECT_THAT(i64_ne(22_u64, 22_u64), Result(0));
}

TEST(execute_numeric, i64_lt_s)
{
    const auto i64_lt_s = create_binary_operation_executor(Instr::i64_lt_s);
    EXPECT_THAT(i64_lt_s(22_u64, 20_u64), Result(0));
    EXPECT_THAT(i64_lt_s(20_u64, 22_u64), Result(1));
    EXPECT_THAT(i64_lt_s(-41_u64, -42_u64), Result(0));
    EXPECT_THAT(i64_lt_s(-42_u64, -41_u64), Result(1));
}

TEST(execute_numeric, i64_lt_u)
{
    const auto i64_lt_u = create_binary_operation_executor(Instr::i64_lt_u);
    EXPECT_THAT(i64_lt_u(22_u64, 20_u64), Result(0));
    EXPECT_THAT(i64_lt_u(20_u64, 22_u64), Result(1));
}

TEST(execute_numeric, i64_gt_s)
{
    const auto i64_gt_s = create_binary_operation_executor(Instr::i64_gt_s);
    EXPECT_THAT(i64_gt_s(22_u64, 20_u64), Result(1));
    EXPECT_THAT(i64_gt_s(20_u64, 22_u64), Result(0));
    EXPECT_THAT(i64_gt_s(-41_u64, -42_u64), Result(1));
    EXPECT_THAT(i64_gt_s(-42_u64, -41_u64), Result(0));
}

TEST(execute_numeric, i64_gt_u)
{
    const auto i64_gt_u = create_binary_operation_executor(Instr::i64_gt_u);
    EXPECT_THAT(i64_gt_u(22_u64, 20_u64), Result(1));
    EXPECT_THAT(i64_gt_u(20_u64, 22_u64), Result(0));
}

TEST(execute_numeric, i64_le_s)
{
    const auto i64_le_s = create_binary_operation_executor(Instr::i64_le_s);
    EXPECT_THAT(i64_le_s(22_u64, 20_u64), Result(0));
    EXPECT_THAT(i64_le_s(20_u64, 22_u64), Result(1));
    EXPECT_THAT(i64_le_s(20_u64, 20_u64), Result(1));
    EXPECT_THAT(i64_le_s(-41_u64, -42_u64), Result(0));
    EXPECT_THAT(i64_le_s(-42_u64, -41_u64), Result(1));
    EXPECT_THAT(i64_le_s(-42_u64, -42_u64), Result(1));
}

TEST(execute_numeric, i64_le_u)
{
    const auto i64_le_u = create_binary_operation_executor(Instr::i64_le_u);
    EXPECT_THAT(i64_le_u(22_u64, 20_u64), Result(0));
    EXPECT_THAT(i64_le_u(20_u64, 22_u64), Result(1));
    EXPECT_THAT(i64_le_u(20_u64, 20_u64), Result(1));
}

TEST(execute_numeric, i64_ge_s)
{
    const auto i64_ge_s = create_binary_operation_executor(Instr::i64_ge_s);
    EXPECT_THAT(i64_ge_s(22_u64, 20_u64), Result(1));
    EXPECT_THAT(i64_ge_s(20_u64, 22_u64), Result(0));
    EXPECT_THAT(i64_ge_s(20_u64, 20_u64), Result(1));
    EXPECT_THAT(i64_ge_s(-41_u64, -42_u64), Result(1));
    EXPECT_THAT(i64_ge_s(-42_u64, -41_u64), Result(0));
    EXPECT_THAT(i64_ge_s(-42_u64, -42_u64), Result(1));
}

TEST(execute_numeric, i64_ge_u)
{
    const auto i64_ge_u = create_binary_operation_executor(Instr::i64_ge_u);
    EXPECT_THAT(i64_ge_u(22_u64, 20_u64), Result(1));
    EXPECT_THAT(i64_ge_u(20_u64, 22_u64), Result(0));
    EXPECT_THAT(i64_ge_u(20_u64, 20_u64), Result(1));
}

TEST(execute_numeric, i32_clz)
{
    const auto i32_clz = create_unary_operation_executor(Instr::i32_clz);
    for (const auto& [input, popcount, countl_zero, countr_zero] : bitcount32_test_cases)
        EXPECT_THAT(i32_clz(input), Result(countl_zero)) << input;
}

TEST(execute_numeric, i32_ctz)
{
    const auto i32_ctz = create_unary_operation_executor(Instr::i32_ctz);
    for (const auto& [input, popcount, countl_zero, countr_zero] : bitcount32_test_cases)
        EXPECT_THAT(i32_ctz(input), Result(countr_zero)) << input;
}

TEST(execute_numeric, i32_popcnt)
{
    const auto i32_popcnt = create_unary_operation_executor(Instr::i32_popcnt);
    for (const auto& [input, popcount, countl_zero, countr_zero] : bitcount32_test_cases)
        EXPECT_THAT(i32_popcnt(input), Result(popcount)) << input;
}

TEST(execute_numeric, i32_add)
{
    const auto i32_add = create_binary_operation_executor(Instr::i32_add);
    EXPECT_THAT(i32_add(22, 20), Result(42));
}

TEST(execute_numeric, i32_sub)
{
    const auto i32_sub = create_binary_operation_executor(Instr::i32_sub);
    EXPECT_THAT(i32_sub(424242, 424200), Result(42));
}

TEST(execute_numeric, i32_mul)
{
    const auto i32_mul = create_binary_operation_executor(Instr::i32_mul);
    EXPECT_THAT(i32_mul(2, 21), Result(42));
}

TEST(execute_numeric, i32_div_s)
{
    const auto i32_div_s = create_binary_operation_executor(Instr::i32_div_s);
    EXPECT_THAT(i32_div_s(uint32_t(-84), 2), Result(-42));
    EXPECT_THAT(i32_div_s(84, 0), Traps());
    EXPECT_THAT(i32_div_s(uint32_t(std::numeric_limits<int32_t>::min()), uint32_t(-1)), Traps());
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
    const auto i32_div_u = create_binary_operation_executor(Instr::i32_div_u);
    EXPECT_THAT(i32_div_u(84, 2), Result(42));
    EXPECT_THAT(i32_div_u(84, 0), Traps());
}

TEST(execute_numeric, i32_rem_s)
{
    const auto i32_rem_s = create_binary_operation_executor(Instr::i32_rem_s);
    EXPECT_THAT(i32_rem_s(uint32_t(-4242), 4200), Result(-42));
    constexpr auto i32_min = std::numeric_limits<int32_t>::min();
    EXPECT_THAT(i32_rem_s(uint32_t(i32_min), uint32_t(-1)), Result(0));
    EXPECT_THAT(i32_rem_s(uint32_t(-4242), 0), Traps());
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
    const auto i32_rem_u = create_binary_operation_executor(Instr::i32_rem_u);
    EXPECT_THAT(i32_rem_u(4242, 4200), Result(42));
    EXPECT_THAT(i32_rem_u(4242, 0), Traps());
}

TEST(execute_numeric, i32_and)
{
    const auto i32_and = create_binary_operation_executor(Instr::i32_and);
    EXPECT_THAT(i32_and(0x00ffff, 0xffff00), Result(0xff00));
}

TEST(execute_numeric, i32_or)
{
    const auto i32_or = create_binary_operation_executor(Instr::i32_or);
    EXPECT_THAT(i32_or(0x00ffff, 0xffff00), Result(0xffffff));
}

TEST(execute_numeric, i32_xor)
{
    const auto i32_xor = create_binary_operation_executor(Instr::i32_xor);
    EXPECT_THAT(i32_xor(0x00ffff, 0xffff00), Result(0xff00ff));
}

TEST(execute_numeric, i32_shl)
{
    const auto i32_shl = create_binary_operation_executor(Instr::i32_shl);
    EXPECT_THAT(i32_shl(21, 1), Result(42));
    EXPECT_THAT(i32_shl(0xffffffff, 0), Result(0xffffffff));
    EXPECT_THAT(i32_shl(0xffffffff, 1), Result(0xfffffffe));
    EXPECT_THAT(i32_shl(0xffffffff, 31), Result(0x80000000));
    EXPECT_THAT(i32_shl(0xffffffff, 32), Result(0xffffffff));
    EXPECT_THAT(i32_shl(0xffffffff, 33), Result(0xfffffffe));
    EXPECT_THAT(i32_shl(0xffffffff, 63), Result(0x80000000));
}

TEST(execute_numeric, i32_shr_s)
{
    const auto i32_shr_s = create_binary_operation_executor(Instr::i32_shr_s);
    EXPECT_THAT(i32_shr_s(uint32_t(-84), 1), Result(-42));
    EXPECT_THAT(i32_shr_s(0xffffffff, 0), Result(0xffffffff));
    EXPECT_THAT(i32_shr_s(0xffffffff, 1), Result(0xffffffff));
    EXPECT_THAT(i32_shr_s(0xffffffff, 31), Result(0xffffffff));
    EXPECT_THAT(i32_shr_s(0xffffffff, 32), Result(0xffffffff));
    EXPECT_THAT(i32_shr_s(0xffffffff, 33), Result(0xffffffff));
    EXPECT_THAT(i32_shr_s(0xffffffff, 63), Result(0xffffffff));
    EXPECT_THAT(i32_shr_s(0x7fffffff, 0), Result(0x7fffffff));
    EXPECT_THAT(i32_shr_s(0x7fffffff, 1), Result(0x3fffffff));
    EXPECT_THAT(i32_shr_s(0x7fffffff, 30), Result(1));
    EXPECT_THAT(i32_shr_s(0x7fffffff, 31), Result(0));
    EXPECT_THAT(i32_shr_s(0x7fffffff, 32), Result(0x7fffffff));
    EXPECT_THAT(i32_shr_s(0x7fffffff, 33), Result(0x3fffffff));
    EXPECT_THAT(i32_shr_s(0x7fffffff, 62), Result(1));
    EXPECT_THAT(i32_shr_s(0x7fffffff, 63), Result(0));
    EXPECT_THAT(i32_shr_s(1, uint32_t(-1)), Result(0));
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
    const auto i32_shr_u = create_binary_operation_executor(Instr::i32_shr_u);
    EXPECT_THAT(i32_shr_u(84, 1), Result(42));
    EXPECT_THAT(i32_shr_u(0xffffffff, 0), Result(0xffffffff));
    EXPECT_THAT(i32_shr_u(0xffffffff, 1), Result(0x7fffffff));
    EXPECT_THAT(i32_shr_u(0xffffffff, 31), Result(1));
    EXPECT_THAT(i32_shr_u(0xffffffff, 32), Result(0xffffffff));
    EXPECT_THAT(i32_shr_u(0xffffffff, 33), Result(0x7fffffff));
    EXPECT_THAT(i32_shr_u(0xffffffff, 63), Result(1));
}

TEST(execute_numeric, i32_rotl)
{
    const auto i32_rotl = create_binary_operation_executor(Instr::i32_rotl);
    EXPECT_THAT(i32_rotl(0xff000000, 0), Result(0xff000000));
    EXPECT_THAT(i32_rotl(0xff000000, 1), Result(0xfe000001));
    EXPECT_THAT(i32_rotl(0xff000000, 31), Result(0x7f800000));
    EXPECT_THAT(i32_rotl(0xff000000, 32), Result(0xff000000));
    EXPECT_THAT(i32_rotl(0xff000000, 33), Result(0xfe000001));
    EXPECT_THAT(i32_rotl(0xff000000, 63), Result(0x7f800000));
}

TEST(execute_numeric, i32_rotr)
{
    const auto i32_rotr = create_binary_operation_executor(Instr::i32_rotr);
    EXPECT_THAT(i32_rotr(0x000000ff, 0), Result(0x000000ff));
    EXPECT_THAT(i32_rotr(0x000000ff, 1), Result(0x8000007f));
    EXPECT_THAT(i32_rotr(0x000000ff, 31), Result(0x000001fe));
    EXPECT_THAT(i32_rotr(0x000000ff, 32), Result(0x000000ff));
    EXPECT_THAT(i32_rotr(0x000000ff, 33), Result(0x8000007f));
    EXPECT_THAT(i32_rotr(0x000000ff, 63), Result(0x000001fe));
}

TEST(execute_numeric, i32_wrap_i64)
{
    const auto i32_wrap_i64 = create_unary_operation_executor(Instr::i32_wrap_i64);
    // <=32-bits set
    EXPECT_THAT(i32_wrap_i64(0xffffffff_u64), Result(0xffffffff));
    // >32-bits set
    EXPECT_THAT(i32_wrap_i64(0xffffffffffffffff_u64), Result(0xffffffff));
}

TEST(execute_numeric, i64_extend_i32_s)
{
    const auto i64_extend_i32_s = create_unary_operation_executor(Instr::i64_extend_i32_s);
    EXPECT_THAT(i64_extend_i32_s(0x00000000_u32), Result(0x0000000000000000_u64));
    EXPECT_THAT(i64_extend_i32_s(0x00000001_u32), Result(0x0000000000000001_u64));
    EXPECT_THAT(i64_extend_i32_s(0x7ffffffe_u32), Result(0x000000007ffffffe_u64));
    EXPECT_THAT(i64_extend_i32_s(0x7fffffff_u32), Result(0x000000007fffffff_u64));
    EXPECT_THAT(i64_extend_i32_s(0x80000000_u32), Result(0xffffffff80000000_u64));
    EXPECT_THAT(i64_extend_i32_s(0x80000001_u32), Result(0xffffffff80000001_u64));
    EXPECT_THAT(i64_extend_i32_s(0xfffffffe_u32), Result(0xfffffffffffffffe_u64));
    EXPECT_THAT(i64_extend_i32_s(0xffffffff_u32), Result(0xffffffffffffffff_u64));

    // Put some garbage in the Value's high bits.
    TypedValue v{ValType::i32, {}};
    v.value.i64 = 0xdeaddeaddeaddead;
    v.value.i32 = 0x80000000;
    EXPECT_THAT(i64_extend_i32_s(v), Result(0xffffffff80000000_u64));

    v.value.i64 = 0xdeaddeaddeaddead;
    v.value.i32 = 0x40000000;
    EXPECT_THAT(i64_extend_i32_s(v), Result(0x0000000040000000_u64));
}

TEST(execute_numeric, i64_extend_i32_u)
{
    const auto i64_extend_i32_u = create_unary_operation_executor(Instr::i64_extend_i32_u);
    EXPECT_THAT(i64_extend_i32_u(0x00000000_u32), Result(0x0000000000000000_u64));
    EXPECT_THAT(i64_extend_i32_u(0x00000001_u32), Result(0x0000000000000001_u64));
    EXPECT_THAT(i64_extend_i32_u(0x7ffffffe_u32), Result(0x000000007ffffffe_u64));
    EXPECT_THAT(i64_extend_i32_u(0x7fffffff_u32), Result(0x000000007fffffff_u64));
    EXPECT_THAT(i64_extend_i32_u(0x80000000_u32), Result(0x0000000080000000_u64));
    EXPECT_THAT(i64_extend_i32_u(0x80000001_u32), Result(0x0000000080000001_u64));
    EXPECT_THAT(i64_extend_i32_u(0xfffffffe_u32), Result(0x00000000fffffffe_u64));
    EXPECT_THAT(i64_extend_i32_u(0xffffffff_u32), Result(0x00000000ffffffff_u64));

    // Put some garbage in the Value's high bits.
    TypedValue v{ValType::i32, {}};
    v.value.i64 = 0xdeaddeaddeaddead;
    v.value.i32 = 0x80000000;
    EXPECT_THAT(i64_extend_i32_u(v), Result(0x0000000080000000_u64));
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
    EXPECT_THAT(execute(*instance, 0, {0xff000000}), Result(0x00000000ff000000_u64));
}

TEST(execute_numeric, i64_extend_i32_u_3)
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
    EXPECT_THAT(execute(*instance, 0, {0xff000000}), Result(0x00000000ff000000_u64));
}

TEST(execute_numeric, i64_clz)
{
    const auto i64_clz = create_unary_operation_executor(Instr::i64_clz);
    for (const auto& [input, popcount, countl_zero, countr_zero] : bitcount64_test_cases)
        EXPECT_THAT(i64_clz(input), Result(countl_zero)) << input;
}

TEST(execute_numeric, i64_ctz)
{
    const auto i64_ctz = create_unary_operation_executor(Instr::i64_ctz);
    for (const auto& [input, popcount, countl_zero, countr_zero] : bitcount64_test_cases)
        EXPECT_THAT(i64_ctz(input), Result(countr_zero)) << input;
}

TEST(execute_numeric, i64_popcnt)
{
    const auto i64_popcnt = create_unary_operation_executor(Instr::i64_popcnt);
    for (const auto& [input, popcount, countl_zero, countr_zero] : bitcount64_test_cases)
        EXPECT_THAT(i64_popcnt(input), Result(popcount)) << input;
}

TEST(execute_numeric, i64_add)
{
    const auto i64_add = create_binary_operation_executor(Instr::i64_add);
    EXPECT_THAT(i64_add(22_u64, 20_u64), Result(42));
}

TEST(execute_numeric, i64_sub)
{
    const auto i64_sub = create_binary_operation_executor(Instr::i64_sub);
    EXPECT_THAT(i64_sub(424242_u64, 424200_u64), Result(42));
}

TEST(execute_numeric, i64_mul)
{
    const auto i64_mul = create_binary_operation_executor(Instr::i64_mul);
    EXPECT_THAT(i64_mul(2_u64, 21_u64), Result(42));
}

TEST(execute_numeric, i64_div_s)
{
    const auto i64_div_s = create_binary_operation_executor(Instr::i64_div_s);
    EXPECT_THAT(i64_div_s(-84_u64, 2_u64), Result(-42_u64));
    EXPECT_THAT(i64_div_s(84_u64, 0_u64), Traps());
    EXPECT_THAT(i64_div_s(uint64_t(std::numeric_limits<int64_t>::min()), -1_u64), Traps());
}

TEST(execute_numeric, i64_div_u)
{
    const auto i64_div_u = create_binary_operation_executor(Instr::i64_div_u);
    EXPECT_THAT(i64_div_u(84_u64, 2_u64), Result(42));
    EXPECT_THAT(i64_div_u(84_u64, 0_u64), Traps());
}

TEST(execute_numeric, i64_rem_s)
{
    const auto i64_rem_s = create_binary_operation_executor(Instr::i64_rem_s);
    EXPECT_THAT(i64_rem_s(-4242_u64, 4200_u64), Result(-42_u64));
    constexpr auto i64_min = std::numeric_limits<int64_t>::min();
    EXPECT_THAT(i64_rem_s(uint64_t(i64_min), -1_u64), Result(0));
    EXPECT_THAT(i64_rem_s(-4242_u64, 0_u64), Traps());
}

TEST(execute_numeric, i64_rem_u)
{
    const auto i64_rem_u = create_binary_operation_executor(Instr::i64_rem_u);
    EXPECT_THAT(i64_rem_u(4242_u64, 4200_u64), Result(42));
    EXPECT_THAT(i64_rem_u(4242_u64, 0_u64), Traps());
}

TEST(execute_numeric, i64_and)
{
    const auto i64_and = create_binary_operation_executor(Instr::i64_and);
    EXPECT_THAT(i64_and(0x00ffff_u64, 0xffff00_u64), Result(0xff00));
}

TEST(execute_numeric, i64_or)
{
    const auto i64_or = create_binary_operation_executor(Instr::i64_or);
    EXPECT_THAT(i64_or(0x00ffff_u64, 0xffff00_u64), Result(0xffffff));
}

TEST(execute_numeric, i64_xor)
{
    const auto i64_xor = create_binary_operation_executor(Instr::i64_xor);
    EXPECT_THAT(i64_xor(0x00ffff_u64, 0xffff00_u64), Result(0xff00ff));
}

TEST(execute_numeric, i64_shl)
{
    const auto i64_shl = create_binary_operation_executor(Instr::i64_shl);
    EXPECT_THAT(i64_shl(21_u64, 1_u64), Result(42));
    EXPECT_THAT(i64_shl(0xffffffffffffffff_u64, 0_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shl(0xffffffffffffffff_u64, 1_u64), Result(0xfffffffffffffffe));
    EXPECT_THAT(i64_shl(0xffffffffffffffff_u64, 63_u64), Result(0x8000000000000000));
    EXPECT_THAT(i64_shl(0xffffffffffffffff_u64, 64_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shl(0xffffffffffffffff_u64, 65_u64), Result(0xfffffffffffffffe));
    EXPECT_THAT(i64_shl(0xffffffffffffffff_u64, 127_u64), Result(0x8000000000000000));
}

TEST(execute_numeric, i64_shr_s)
{
    const auto i64_shr_s = create_binary_operation_executor(Instr::i64_shr_s);
    EXPECT_THAT(i64_shr_s(-84_u64, 1_u64), Result(-42_u64));
    EXPECT_THAT(i64_shr_s(0xffffffffffffffff_u64, 0_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shr_s(0xffffffffffffffff_u64, 1_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shr_s(0xffffffffffffffff_u64, 63_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shr_s(0xffffffffffffffff_u64, 64_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shr_s(0xffffffffffffffff_u64, 65_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shr_s(0xffffffffffffffff_u64, 127_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shr_s(0x7fffffffffffffff_u64, 0_u64), Result(0x7fffffffffffffff));
    EXPECT_THAT(i64_shr_s(0x7fffffffffffffff_u64, 1_u64), Result(0x3fffffffffffffff));
    EXPECT_THAT(i64_shr_s(0x7fffffffffffffff_u64, 62_u64), Result(1));
    EXPECT_THAT(i64_shr_s(0x7fffffffffffffff_u64, 63_u64), Result(0));
    EXPECT_THAT(i64_shr_s(0x7fffffffffffffff_u64, 64_u64), Result(0x7fffffffffffffff));
    EXPECT_THAT(i64_shr_s(0x7fffffffffffffff_u64, 65_u64), Result(0x3fffffffffffffff));
    EXPECT_THAT(i64_shr_s(0x7fffffffffffffff_u64, 126_u64), Result(1));
    EXPECT_THAT(i64_shr_s(0x7fffffffffffffff_u64, 127_u64), Result(0));
    EXPECT_THAT(i64_shr_s(1_u64, -1_u64), Result(0));
}

TEST(execute_numeric, i64_shr_u)
{
    const auto i64_shr_u = create_binary_operation_executor(Instr::i64_shr_u);
    EXPECT_THAT(i64_shr_u(84_u64, 1_u64), Result(42));
    EXPECT_THAT(i64_shr_u(0xffffffffffffffff_u64, 0_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shr_u(0xffffffffffffffff_u64, 1_u64), Result(0x7fffffffffffffff));
    EXPECT_THAT(i64_shr_u(0xffffffffffffffff_u64, 63_u64), Result(1));
    EXPECT_THAT(i64_shr_u(0xffffffffffffffff_u64, 64_u64), Result(0xffffffffffffffff));
    EXPECT_THAT(i64_shr_u(0xffffffffffffffff_u64, 65_u64), Result(0x7fffffffffffffff));
    EXPECT_THAT(i64_shr_u(0xffffffffffffffff_u64, 127_u64), Result(1));
}

TEST(execute_numeric, i64_rotl)
{
    const auto i64_rotl = create_binary_operation_executor(Instr::i64_rotl);
    EXPECT_THAT(i64_rotl(0xff00000000000000_u64, 0_u64), Result(0xff00000000000000));
    EXPECT_THAT(i64_rotl(0xff00000000000000_u64, 1_u64), Result(0xfe00000000000001));
    EXPECT_THAT(i64_rotl(0xff00000000000000_u64, 63_u64), Result(0x7f80000000000000));
    EXPECT_THAT(i64_rotl(0xff00000000000000_u64, 64_u64), Result(0xff00000000000000));
    EXPECT_THAT(i64_rotl(0xff00000000000000_u64, 65_u64), Result(0xfe00000000000001));
    EXPECT_THAT(i64_rotl(0xff00000000000000_u64, 127_u64), Result(0x7f80000000000000));
}

TEST(execute_numeric, i64_rotr)
{
    const auto i64_rotr = create_binary_operation_executor(Instr::i64_rotr);
    EXPECT_THAT(i64_rotr(0x00000000000000ff_u64, 0_u64), Result(0x00000000000000ff));
    EXPECT_THAT(i64_rotr(0x00000000000000ff_u64, 1_u64), Result(0x800000000000007f));
    EXPECT_THAT(i64_rotr(0x00000000000000ff_u64, 63_u64), Result(0x00000000000001fe));
    EXPECT_THAT(i64_rotr(0x00000000000000ff_u64, 64_u64), Result(0x00000000000000ff));
    EXPECT_THAT(i64_rotr(0x00000000000000ff_u64, 65_u64), Result(0x800000000000007f));
    EXPECT_THAT(i64_rotr(0x00000000000000ff_u64, 127_u64), Result(0x00000000000001fe));
}
