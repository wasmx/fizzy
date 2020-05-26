// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>
#include <test/utils/wasm_binary.hpp>

using namespace fizzy;
using namespace fizzy::test;

namespace
{
const Module ModuleWithSingleFunction = {
    {FuncType{{}, {}}}, {}, {0}, {}, {}, {}, {}, std::nullopt, {}, {}, {}, {}, {}, {}, {}};

inline auto parse_expr(
    const bytes& input, FuncIdx func_idx = 0, const Module& module = ModuleWithSingleFunction)
{
    return fizzy::parse_expr(input.data(), input.data() + input.size(), func_idx, module);
}
}  // namespace

TEST(parser_expr, instr_loop)
{
    const auto loop_void = "03400b0b"_bytes;
    const auto [code1, pos1] = parse_expr(loop_void);
    EXPECT_EQ(code1.instructions, (std::vector{Instr::loop, Instr::end, Instr::end}));
    EXPECT_EQ(code1.immediates.size(), 0);
    EXPECT_EQ(code1.max_stack_height, 0);

    const auto loop_i32 = "037f41000b0b"_bytes;
    const auto [code2, pos2] = parse_expr(loop_i32);
    EXPECT_EQ(
        code2.instructions, (std::vector{Instr::loop, Instr::i32_const, Instr::end, Instr::end}));
    EXPECT_EQ(code2.immediates.size(), 4);
    EXPECT_EQ(code2.max_stack_height, 1);

    const auto loop_f32 = "037d43000000000b0b"_bytes;
    const auto [code3, pos3] = parse_expr(loop_f32);
    EXPECT_EQ(
        code3.instructions, (std::vector{Instr::loop, Instr::f32_const, Instr::end, Instr::end}));
    EXPECT_EQ(code3.immediates.size(), 0);
    EXPECT_EQ(code3.max_stack_height, 1);

    const auto loop_f64 = "037d4400000000000000000b0b"_bytes;
    const auto [code4, pos4] = parse_expr(loop_f64);
    EXPECT_EQ(
        code4.instructions, (std::vector{Instr::loop, Instr::f64_const, Instr::end, Instr::end}));
    EXPECT_EQ(code4.immediates.size(), 0);
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
    EXPECT_EQ(code1.instructions,
        (std::vector{Instr::nop, Instr::nop, Instr::block, Instr::end, Instr::end}));
    EXPECT_EQ(code1.immediates,
        "00"
        "04000000"
        "09000000"_bytes);

    const auto block_i64 = "027e42000b0b"_bytes;
    const auto [code2, pos2] = parse_expr(block_i64);
    EXPECT_EQ(
        code2.instructions, (std::vector{Instr::block, Instr::i64_const, Instr::end, Instr::end}));
    EXPECT_EQ(code2.immediates,
        "01"
        "03000000"
        "11000000"
        "0000000000000000"_bytes);

    const auto block_f64 = "027c4400000000000000000b0b"_bytes;
    const auto [code3, pos3] = parse_expr(block_f64);
    EXPECT_EQ(
        code3.instructions, (std::vector{Instr::block, Instr::f64_const, Instr::end, Instr::end}));
    EXPECT_EQ(code3.immediates,
        "01"
        "03000000"
        "09000000"_bytes);
}

TEST(parser_expr, instr_block_input_buffer_overflow)
{
    // The function end opcode 0b is missing causing reading out of input buffer.
    const auto block_missing_end = "02400b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(block_missing_end), parser_error, "unexpected EOF");
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
    // end

    const auto code_bin = "010240410a21010c00410b21010b20010b"_bytes;
    const auto [code, pos] = parse_expr(code_bin);
    EXPECT_EQ(code.instructions,
        (std::vector{Instr::nop, Instr::block, Instr::i32_const, Instr::local_set, Instr::br,
            Instr::i32_const, Instr::local_set, Instr::end, Instr::local_get, Instr::end}));
    EXPECT_EQ(code.immediates,
        "00"
        "08000000"
        "1d000000"
        "0a000000"
        "01000000"
        "00000000"
        "0b000000"
        "01000000"
        "01000000"_bytes);
    EXPECT_EQ(code.max_stack_height, 1);
}

TEST(parser_expr, instr_br_table)
{
    /*
     (block
       (block
         (block
           (block
             (block
               (br_table 3 2 1 0 4 (get_local 0))
               (return (i32.const 99))
             )
             (return (i32.const 100))
           )
           (return (i32.const 101))
         )
         (return (i32.const 102))
       )
       (return (i32.const 103))
     )
     (i32.const 104)
    */

    const auto code_bin =
        "0240024002400240024020000e04030201000441e3"
        "000f0b41e4000f0b41e5000f0b41e6000f0b41e7000f0b41e8000b000c04"
        "6e616d6502050100010000"_bytes;

    const auto [code, pos] = parse_expr(code_bin);

    EXPECT_EQ(code.instructions,
        (std::vector{Instr::block, Instr::block, Instr::block, Instr::block, Instr::block,
            Instr::local_get, Instr::br_table, Instr::i32_const, Instr::return_, Instr::end,
            Instr::i32_const, Instr::return_, Instr::end, Instr::i32_const, Instr::return_,
            Instr::end, Instr::i32_const, Instr::return_, Instr::end, Instr::i32_const,
            Instr::return_, Instr::end, Instr::i32_const, Instr::end}));

    // 5 blocks + 1 local_get before br_table
    const auto br_table_imm_offset = 5 * (1 + 2 * 4) + 4;
    const auto expected_br_imm =
        "04000000"
        "03000000"
        "02000000"
        "01000000"
        "00000000"
        "04000000"_bytes;
    EXPECT_EQ(code.immediates.substr(br_table_imm_offset, expected_br_imm.size()), expected_br_imm);
    EXPECT_EQ(code.max_stack_height, 1);
}

TEST(parser_expr, instr_br_table_empty_vector)
{
    /*
      (block
        (br_table 0 (get_local 0))
        (return (i32.const 99))
      )
      (i32.const 100)
    */

    const auto code_bin = "024020000e000041e3000f0b41e4000b000c046e616d6502050100010000"_bytes;

    const auto [code, pos] = parse_expr(code_bin);

    EXPECT_EQ(code.instructions,
        (std::vector{Instr::block, Instr::local_get, Instr::br_table, Instr::i32_const,
            Instr::return_, Instr::end, Instr::i32_const, Instr::end}));

    // blocks + local_get before br_table
    const auto br_table_imm_offset = 1 + 2 * 4 + 4;
    const auto expected_br_imm =
        "00000000"
        "00000000"_bytes;
    EXPECT_EQ(code.immediates.substr(br_table_imm_offset, expected_br_imm.size()), expected_br_imm);
    EXPECT_EQ(code.max_stack_height, 1);
}

TEST(parser_expr, instr_br_table_as_return)
{
    /*
       i32.const 0
       br_table 0
    */

    const auto code_bin = "41000e00000b"_bytes;
    const auto [code, _] = parse_expr(code_bin);
    EXPECT_EQ(code.instructions, (std::vector{Instr::i32_const, Instr::br_table, Instr::end}));
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
    const auto [code, pos] = parse_expr(code1_bin, 0, module);
    EXPECT_EQ(code.instructions, (std::vector{Instr::i32_const, Instr::call_indirect, Instr::end}));

    const auto code2_bin = i32_const(0) + "1100010b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(code2_bin, 0, module), parser_error,
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
        EXPECT_THROW_MESSAGE(parse_expr(code, 0, module), parser_error, "unexpected EOF");
    }
}

TEST(parser_expr, immediate_float_out_of_bounds)
{
    const auto expr_f32_const = bytes{uint8_t(Instr::f32_const), 0x01};
    EXPECT_THROW_MESSAGE(parse_expr(expr_f32_const), parser_error, "unexpected EOF");
    const auto expr_f64_const =
        bytes{uint8_t(Instr::f64_const), 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07};
    EXPECT_THROW_MESSAGE(parse_expr(expr_f64_const), parser_error, "unexpected EOF");
}

TEST(parser_expr, load_store_immediates_out_of_bounds)
{
    for (const auto instr : {Instr::i32_load, Instr::i64_load, Instr::i32_load8_s,
             Instr::i32_load8_u, Instr::i32_load16_s, Instr::i32_load16_u, Instr::i64_load8_s,
             Instr::i64_load8_u, Instr::i64_load16_s, Instr::i64_load16_u, Instr::i64_load32_s,
             Instr::i64_load32_u, Instr::i32_store, Instr::i64_store, Instr::i32_store8,
             Instr::i32_store16, Instr::i64_store8, Instr::i64_store16, Instr::i64_store32})
    {
        const auto code_imm1 = i32_const(0) + i32_const(0) + bytes{uint8_t(instr), 0xa0};
        EXPECT_THROW_MESSAGE(parse_expr(code_imm1), parser_error, "unexpected EOF");
        const auto code_imm2 = i32_const(0) + i32_const(0) + bytes{uint8_t(instr), 0x00, 0xb0};
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
        parse_expr(i32_const(0) + "1100"_bytes, 0, module), parser_error, "unexpected EOF");
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
    ASSERT_EQ(module.codesec.size(), 2);
    EXPECT_EQ(module.codesec[0].max_stack_height, 1);
    EXPECT_EQ(module.codesec[1].max_stack_height, 1);
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
    ASSERT_EQ(module.codesec.size(), 2);
    EXPECT_EQ(module.codesec[0].max_stack_height, 1);
    EXPECT_EQ(module.codesec[1].max_stack_height, 1);
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
