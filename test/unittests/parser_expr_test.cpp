// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <gmock/gmock.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>
#include <test/utils/wasm_binary.hpp>

using namespace fizzy;
using namespace fizzy::test;
using namespace testing;

namespace
{
const Module ModuleWithSingleFunction = {
    {FuncType{{}, {}}}, {}, {0}, {}, {}, {}, {}, std::nullopt, {}, {}, {}, {}, {}, {}, {}};

inline auto parse_expr(bytes_view input, FuncIdx func_idx = 0,
    const std::vector<Locals>& locals = {}, const Module& module = ModuleWithSingleFunction)
{
    return fizzy::parse_expr(input.data(), input.end(), func_idx, locals, module);
}
}  // namespace

TEST(parser_expr, instr_loop)
{
    const auto loop_void = "03400b0b"_bytes;
    const auto [code1, pos1] = parse_expr(loop_void);
    EXPECT_THAT(code1.instructions, ElementsAre(Instr::end));
    EXPECT_EQ(code1.max_stack_height, 0);

    const auto loop_i32 = "037f41000b1a0b"_bytes;
    const auto [code2, pos2] = parse_expr(loop_i32);
    EXPECT_THAT(
        code2.instructions, ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::drop, Instr::end));
    EXPECT_EQ(code2.max_stack_height, 1);

    const auto loop_f32 = "037d43000000000b1a0b"_bytes;
    const auto [code3, pos3] = parse_expr(loop_f32);
    EXPECT_THAT(
        code3.instructions, ElementsAre(Instr::f32_const, 0, 0, 0, 0, Instr::drop, Instr::end));
    EXPECT_EQ(code3.max_stack_height, 1);

    const auto loop_f64 = "037c4400000000000000000b1a0b"_bytes;
    const auto [code4, pos4] = parse_expr(loop_f64);
    EXPECT_THAT(code4.instructions,
        ElementsAre(Instr::f64_const, 0, 0, 0, 0, 0, 0, 0, 0, Instr::drop, Instr::end));
    EXPECT_EQ(code4.max_stack_height, 1);
}

TEST(parser_expr, instr_loop_input_buffer_overflow)
{
    // The function end opcode 0b is missing causing reading out of input buffer.
    const auto loop_missing_end = "03400b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(loop_missing_end), parser_error, "unexpected EOF");
}

TEST(parser_expr, instr_block)
{
    const auto wrong_type = "0200"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(wrong_type), parser_error, "invalid valtype 0");

    const auto empty = "010102400b0b"_bytes;
    const auto [code1, pos1] = parse_expr(empty);
    EXPECT_THAT(code1.instructions, ElementsAre(Instr::end));

    const auto block_i64 = "027e42000b1a0b"_bytes;
    const auto [code2, pos2] = parse_expr(block_i64);
    EXPECT_THAT(code2.instructions,
        ElementsAre(Instr::i64_const, 0, 0, 0, 0, 0, 0, 0, 0, Instr::drop, Instr::end));

    const auto block_f64 = "027c4400000000000000000b1a0b"_bytes;
    const auto [code3, pos3] = parse_expr(block_f64);
    EXPECT_THAT(code3.instructions,
        ElementsAre(Instr::f64_const, 0, 0, 0, 0, 0, 0, 0, 0, Instr::drop, Instr::end));
}

TEST(parser_expr, instr_block_input_buffer_overflow)
{
    // The function end opcode 0b is missing causing reading out of input buffer.
    const auto block_missing_end = "02400b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(block_missing_end), parser_error, "unexpected EOF");
}

TEST(parser_expr, loop_br)
{
    /* wat2wasm
    (func (loop (br 0)))
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0901070003400c000b0b");
    const auto module = parse(wasm);

    EXPECT_THAT(module->codesec[0].instructions,
        ElementsAre(Instr::br, /*arity:*/ 0, 0, 0, 0, /*code_offset:*/ 0, 0, 0, 0,
            /*stack_drop:*/ 0, 0, 0, 0, Instr::end));

    /* wat2wasm
    (func
        (i32.const 0)
        (loop (br 0))
        drop
    )
    */
    const auto wasm_parent_stack =
        from_hex("0061736d01000000010401600000030201000a0c010a00410003400c000b1a0b");
    const auto module_parent_stack = parse(wasm_parent_stack);

    EXPECT_THAT(module_parent_stack->codesec[0].instructions,
        ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::br, /*arity:*/ 0, 0, 0, 0,
            /*code_offset:*/ 5, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0, Instr::drop, Instr::end));

    /* wat2wasm
    (func
        (loop (result i32)
            (i32.const 0)
            (br 0)
        )
        drop
    )
    */
    const auto wasm_arity =
        from_hex("0061736d01000000010401600000030201000a0c010a00037f41000c000b1a0b");
    const auto module_arity = parse(wasm_arity);

    EXPECT_THAT(module_arity->codesec[0].instructions,
        ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::br, /*arity:*/ 0, 0, 0, 0,
            /*code_offset:*/ 0, 0, 0, 0, /*stack_drop:*/ 1, 0, 0, 0, Instr::drop, Instr::end));
}

TEST(parser_expr, loop_return)
{
    /* wat2wasm
    (func (loop (return)))
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0801060003400f0b0b");
    const auto module = parse(wasm);

    EXPECT_THAT(module->codesec[0].instructions,
        ElementsAre(Instr::return_, /*arity:*/ 0, 0, 0, 0, /*code_offset:*/ 13, 0, 0, 0,
            /*stack_drop:*/ 0, 0, 0, 0, Instr::end));
}

TEST(parser_expr, block_br)
{
    // nop
    // block
    //   i32.const 0xa
    //   local.set 1
    //   br 0
    //   i32.const 0xb
    //   local.set 1
    // end
    // local.get 1
    // drop
    // end

    const auto code_bin = "010240410a21010c00410b21010b20011a0b"_bytes;
    const auto [code, pos] = parse_expr(code_bin, 0, {{2, ValType::i32}});
    EXPECT_THAT(code.instructions,
        ElementsAre(Instr::i32_const, 0x0a, 0, 0, 0, Instr::local_set, 1, 0, 0, 0, Instr::br,
            /*arity:*/ 0, 0, 0, 0, /*code_offset:*/ 33, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            Instr::i32_const, 0x0b, 0, 0, 0, Instr::local_set, 1, 0, 0, 0, Instr::local_get, 1, 0,
            0, 0, Instr::drop, Instr::end));
    EXPECT_EQ(code.max_stack_height, 1);

    /* wat2wasm
    (func
        (i32.const 0)
        (block (br 0))
        drop
    )
    */
    const auto wasm_parent_stack =
        from_hex("0061736d01000000010401600000030201000a0c010a00410002400c000b1a0b");
    const auto module_parent_stack = parse(wasm_parent_stack);

    EXPECT_THAT(module_parent_stack->codesec[0].instructions,
        ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::br, /*arity:*/ 0, 0, 0, 0,
            /*code_offset:*/ 18, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0, Instr::drop, Instr::end));

    /* wat2wasm
    (func
        (block (result i32)
            (i32.const 0)
            (br 0)
        )
        drop
    )
    */
    const auto wasm_arity =
        from_hex("0061736d01000000010401600000030201000a0c010a00027f41000c000b1a0b");
    const auto module_arity = parse(wasm_arity);

    EXPECT_THAT(module_arity->codesec[0].instructions,
        ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::br, /*arity:*/ 1, 0, 0, 0,
            /*code_offset:*/ 18, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0, Instr::drop, Instr::end));
}

TEST(parser_expr, block_return)
{
    /* wat2wasm
    (func (block (return)))
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0801060002400f0b0b");
    const auto module = parse(wasm);

    EXPECT_THAT(module->codesec[0].instructions,
        ElementsAre(Instr::return_, /*arity:*/ 0, 0, 0, 0, /*code_offset:*/ 13, 0, 0, 0,
            /*stack_drop:*/ 0, 0, 0, 0, Instr::end));
}

TEST(parser_expr, if_br)
{
    /* wat2wasm
    (func
        (i32.const 0)
        (if (then (br 0)))
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0b010900410004400c000b0b");
    const auto module = parse(wasm);

    EXPECT_THAT(module->codesec[0].instructions,
        ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::if_, /*else_offset:*/ 23, 0, 0, 0,
            Instr::br, /*arity:*/ 0, 0, 0, 0, /*code_offset:*/ 23, 0, 0, 0,
            /*stack_drop:*/ 0, 0, 0, 0, /*23:*/ Instr::end));

    /* wat2wasm
    (func
        (i32.const 0)
        (i32.const 0)
        (if (then (br 0)))
        drop
    )
    */
    const auto wasm_parent_stack =
        from_hex("0061736d01000000010401600000030201000a0e010c004100410004400c000b1a0b");
    const auto module_parent_stack = parse(wasm_parent_stack);

    EXPECT_THAT(module_parent_stack->codesec[0].instructions,
        ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::i32_const, 0, 0, 0, 0, Instr::if_,
            /*else_offset:*/ 28, 0, 0, 0, Instr::br, /*arity:*/ 0, 0, 0, 0,
            /*code_offset:*/ 28, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*28:*/ Instr::drop, Instr::end));
}

TEST(parser_expr, instr_br_table)
{
    /* wat2wasm
    (func (param i32) (result i32)
      (block
        (block
          (block
            (block
              (block
                (br_table 3 2 1 0 4 (get_local 0))
                (return (i32.const 0x41))
              )
              (return (i32.const 0x42))
            )
            (return (i32.const 0x43))
          )
          (return (i32.const 0x44))
        )
        (return (i32.const 0x45))
      )
      (i32.const 0x46)
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000a330131000240024002400240024020000e04030201000441"
        "c1000f0b41c2000f0b41c3000f0b41c4000f0b41c5000f0b41c6000b");

    const auto module = parse(wasm);
    ASSERT_EQ(module->codesec.size(), 1);
    const auto& code = module->codesec[0];

    EXPECT_THAT(code.instructions,
        ElementsAre(Instr::local_get, 0, 0, 0, 0, Instr::br_table,
            /*label_count:*/ 4, 0, 0, 0, /*arity:*/ 0, 0, 0, 0,
            /*code_offset:*/ 126, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*code_offset:*/ 108, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*code_offset:*/ 90, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*code_offset:*/ 72, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*code_offset:*/ 144, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,

            /*54:*/ Instr::i32_const, 0x41, 0, 0, 0, Instr::return_, /*arity:*/ 1, 0, 0, 0,
            /*code_offset:*/ 149, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*72:*/ Instr::i32_const, 0x42, 0, 0, 0, Instr::return_, /*arity:*/ 1, 0, 0, 0,
            /*code_offset:*/ 149, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*90:*/ Instr::i32_const, 0x43, 0, 0, 0, Instr::return_, /*arity:*/ 1, 0, 0, 0,
            /*code_offset:*/ 149, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*108:*/ Instr::i32_const, 0x44, 0, 0, 0, Instr::return_, /*arity:*/ 1, 0, 0, 0,
            /*code_offset:*/ 149, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*126:*/ Instr::i32_const, 0x45, 0, 0, 0, Instr::return_, /*arity:*/ 1, 0, 0, 0,
            /*code_offset:*/ 149, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            /*144:*/ Instr::i32_const, 0x46, 0, 0, 0,
            /*149:*/ Instr::end));

    EXPECT_EQ(code.max_stack_height, 1);
}

TEST(parser_expr, instr_br_table_empty_vector)
{
    /* wat2wasm
    (func (param i32) (result i32)
      (block
        (br_table 0 (get_local 0))
        (return (i32.const 99))
      )
      (i32.const 100)
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000a13011100024020000e000041e3000f0b41e4000b");

    const auto module = parse(wasm);
    ASSERT_EQ(module->codesec.size(), 1);
    const auto& code = module->codesec[0];

    EXPECT_THAT(code.instructions,
        ElementsAre(Instr::local_get, 0, 0, 0, 0, Instr::br_table,
            /*label_count:*/ 0, 0, 0, 0, /*arity:*/ 0, 0, 0, 0, /*code_offset:*/ 40, 0, 0, 0,
            /*stack_drop:*/ 0, 0, 0, 0, Instr::i32_const, 0x63, 0, 0, 0, Instr::return_,
            /*arity:*/ 1, 0, 0, 0, /*code_offset:*/ 45, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            Instr::i32_const, 0x64, 0, 0, 0, Instr::end));

    EXPECT_EQ(code.max_stack_height, 1);
}

TEST(parser_expr, instr_br_table_as_return)
{
    /*
       i32.const 0
       br_table 0
    */

    const auto code_bin = i32_const(0) + "0e00000b"_bytes;
    const auto [code, _] = parse_expr(code_bin);
    EXPECT_THAT(code.instructions,
        ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::br_table, /*label_count:*/ 0, 0, 0, 0,
            /*arity:*/ 0, 0, 0, 0, /*code_offset:*/ 22, 0, 0, 0, /*stack_drop:*/ 0, 0, 0, 0,
            Instr::end));
    EXPECT_EQ(code.max_stack_height, 1);
}

TEST(parser_expr, instr_br_table_missing_arg)
{
    /*
       br_table 0
    */

    const auto code_bin = "0e00000b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(code_bin), validation_error, "stack underflow");
}

TEST(parser_expr, unexpected_else)
{
    // (else)
    const auto code1_bin = "050b0b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(code1_bin), parser_error,
        "unexpected else instruction (if instruction missing)");

    // (block (else))
    const auto code2_bin = "0240050b0b0b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(code2_bin), parser_error,
        "unexpected else instruction (if instruction missing)");
}

TEST(parser_expr, call_indirect_table_index)
{
    Module module;
    module.typesec.emplace_back(FuncType{{}, {}});
    module.funcsec.emplace_back(0);
    module.tablesec.emplace_back(Table{{1, 1}});

    const auto code1_bin = i32_const(0) + "1100000b"_bytes;
    const auto [code, pos] = parse_expr(code1_bin, 0, {}, module);
    EXPECT_THAT(code.instructions,
        ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::call_indirect, 0, 0, 0, 0, Instr::end));

    const auto code2_bin = i32_const(0) + "1100010b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(code2_bin, 0, {}, module), parser_error,
        "invalid tableidx encountered with call_indirect");
}

TEST(parser_expr, control_instr_out_of_bounds)
{
    EXPECT_THROW_MESSAGE(parse_expr("02"_bytes), parser_error, "unexpected EOF");
    EXPECT_THROW_MESSAGE(parse_expr("03"_bytes), parser_error, "unexpected EOF");
    EXPECT_THROW_MESSAGE(parse_expr(i32_const(0) + "04"_bytes), parser_error, "unexpected EOF");
}

TEST(parser_expr, immediate_leb128_out_of_bounds)
{
    Module module;
    module.typesec.emplace_back(FuncType{{}, {}});
    module.funcsec.emplace_back(0);
    module.tablesec.emplace_back(Table{{1, 1}});  // needed for call_indirect

    for (const auto instr : {Instr::local_get, Instr::local_set, Instr::local_tee,
             Instr::global_get, Instr::global_set, Instr::br, Instr::br_if, Instr::call,
             Instr::call_indirect, Instr::i32_const, Instr::i64_const})
    {
        const auto code = i32_const(0) + i32_const(0) + bytes{uint8_t(instr), 0x99};
        EXPECT_THROW_MESSAGE(parse_expr(code, 0, {}, module), parser_error, "unexpected EOF");
    }
}

TEST(parser_expr, immediate_float_out_of_bounds)
{
    // These tests use exact-fit array to detect invalid pointer comparisons in parser.

    const uint8_t expr_f32_const[]{uint8_t(Instr::f32_const), 0x01};
    EXPECT_THROW_MESSAGE(parse_expr(bytes_view{expr_f32_const, std::size(expr_f32_const)}),
        parser_error, "unexpected EOF");

    const uint8_t expr_f64_const_2[]{uint8_t(Instr::f64_const), 0x01, 0x02};
    EXPECT_THROW_MESSAGE(parse_expr(bytes_view{expr_f64_const_2, std::size(expr_f64_const_2)}),
        parser_error, "unexpected EOF");

    const uint8_t expr_f64_const_7[]{
        uint8_t(Instr::f64_const), 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    EXPECT_THROW_MESSAGE(parse_expr(bytes_view{expr_f64_const_7, std::size(expr_f64_const_7)}),
        parser_error, "unexpected EOF");
}

TEST(parser_expr, load_store_immediates_out_of_bounds)
{
    for (const auto instr : {Instr::i32_load, Instr::i64_load, Instr::i32_load8_s,
             Instr::i32_load8_u, Instr::i32_load16_s, Instr::i32_load16_u, Instr::i64_load8_s,
             Instr::i64_load8_u, Instr::i64_load16_s, Instr::i64_load16_u, Instr::i64_load32_s,
             Instr::i64_load32_u, Instr::i32_store, Instr::i32_store8, Instr::i32_store16})
    {
        const auto code_imm1 = i32_const(0) + i32_const(0) + bytes{uint8_t(instr), 0xa0};
        EXPECT_THROW_MESSAGE(parse_expr(code_imm1), parser_error, "unexpected EOF");
        const auto code_imm2 = i32_const(0) + i32_const(0) + bytes{uint8_t(instr), 0x00, 0xb0};
        EXPECT_THROW_MESSAGE(parse_expr(code_imm2), parser_error, "unexpected EOF");
    }

    for (const auto instr :
        {Instr::i64_store, Instr::i64_store8, Instr::i64_store16, Instr::i64_store32})
    {
        const auto code_imm1 = i32_const(0) + i64_const(0) + bytes{uint8_t(instr), 0xa0};
        EXPECT_THROW_MESSAGE(parse_expr(code_imm1), parser_error, "unexpected EOF");
        const auto code_imm2 = i32_const(0) + i64_const(0) + bytes{uint8_t(instr), 0x00, 0xb0};
        EXPECT_THROW_MESSAGE(parse_expr(code_imm2), parser_error, "unexpected EOF");
    }
}

TEST(parser_expr, br_table_out_of_bounds)
{
    EXPECT_THROW_MESSAGE(parse_expr(i32_const(0) + "0e008f"_bytes), parser_error, "unexpected EOF");
    EXPECT_THROW_MESSAGE(parse_expr(i32_const(0) + "0e018f"_bytes), parser_error, "unexpected EOF");
    EXPECT_THROW_MESSAGE(parse_expr(i32_const(0) + "0e0201"_bytes), parser_error, "unexpected EOF");
    EXPECT_THROW_MESSAGE(
        parse_expr(i32_const(0) + "0e02018f"_bytes), parser_error, "unexpected EOF");
}

TEST(parser_expr, call_indirect_out_of_bounds)
{
    Module module;
    module.typesec.emplace_back(FuncType{{}, {}});
    module.funcsec.emplace_back(0);
    module.tablesec.emplace_back(Table{{1, 1}});

    EXPECT_THROW_MESSAGE(
        parse_expr(i32_const(0) + "1100"_bytes, 0, {}, module), parser_error, "unexpected EOF");
}

TEST(parser_expr, memory_grow_out_of_bounds)
{
    for (const auto instr : {Instr::memory_size, Instr::memory_grow})
    {
        const auto code = i32_const(0) + uint8_t(instr);
        EXPECT_THROW_MESSAGE(parse_expr(code), parser_error, "unexpected EOF");
    }
}

TEST(parser_expr, call_0args_1result)
{
    /* wat2wasm
    (func (result i32) (i32.const 0))
    (func (result i32) (call 0))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f03030200000a0b02040041000b040010000b");

    const auto module = parse(wasm);
    ASSERT_EQ(module->codesec.size(), 2);
    EXPECT_EQ(module->codesec[0].max_stack_height, 1);
    EXPECT_EQ(module->codesec[1].max_stack_height, 1);
}

TEST(parser_expr, call_1arg_1result)
{
    /* wat2wasm
    (func (param i32) (result i32) (local.get 0))
    (func (result i32) (call 0 (i32.const 0)))
    */
    const auto wasm = from_hex(
        "0061736d01000000010a0260017f017f6000017f03030200010a0d02040020000b0600410010000b");

    const auto module = parse(wasm);
    ASSERT_EQ(module->codesec.size(), 2);
    EXPECT_EQ(module->codesec[0].max_stack_height, 1);
    EXPECT_EQ(module->codesec[1].max_stack_height, 1);
}

TEST(parser_expr, call_nonexisting_typeidx)
{
    // This creates a wasm module where code[0] has a call instruction calling function[1] which
    // has invalid type_idx 1.
    // wat2wasm cannot be used as there is no way to have invalid type_idx in WAT form.
    const auto wasm = bytes{wasm_prefix} + make_section(1, make_vec({"600000"_bytes})) +
                      make_section(3, make_vec({"00"_bytes, "01"_bytes})) +
                      make_section(10, make_vec({"040010010b"_bytes, "02000b"_bytes}));

    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid function type index");
}

TEST(parser_expr, nop_like_instructions_are_skipped)
{
    /* wat2wasm
    (func
      nop
      (block
        nop
        (loop
          nop
          (block nop)
          nop
        )
        nop
      )
      nop
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a14011200010240010340010240010b010b010b010b");

    const auto module = parse(wasm);
    ASSERT_EQ(module->codesec.size(), 1);
    EXPECT_THAT(module->codesec[0].instructions, ElementsAre(Instr::end));
}
