#include "parser.hpp"
#include "utils.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

static const auto functype_void_to_void = from_hex("600000");
static const auto functype_i32i64_to_i32 = from_hex("60027f7e017f");
static const auto functype_i32_to_void = from_hex("60017f00");

TEST(parser, valtype)
{
    uint8_t b;
    b = 0x7e;
    EXPECT_EQ(std::get<0>(parser<ValType>{}(&b)), ValType::i64);
    b = 0x7f;
    EXPECT_EQ(std::get<0>(parser<ValType>{}(&b)), ValType::i32);
    b = 0x7d;
    EXPECT_THROW(std::get<0>(parser<ValType>{}(&b)), parser_error);
}

TEST(parser, valtype_vec)
{
    const auto input = from_hex("037f7e7fcc");
    const auto [vec, pos] = parser<std::vector<ValType>>{}(input.data());
    EXPECT_EQ(pos, input.data() + 4);
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], ValType::i32);
    EXPECT_EQ(vec[1], ValType::i64);
    EXPECT_EQ(vec[2], ValType::i32);
}

TEST(parser, limits_min)
{
    const auto input = from_hex("007f");
    const auto [limits, pos] = parser<Limits>{}(input.data());
    EXPECT_EQ(limits.min, 0x7f);
    EXPECT_EQ(limits.max, std::numeric_limits<uint32_t>::max());
}

TEST(parser, limits_minmax)
{
    const auto input = from_hex("01207f");
    const auto [limits, pos] = parser<Limits>{}(input.data());
    EXPECT_EQ(limits.min, 0x20);
    EXPECT_EQ(limits.max, 0x7f);
}

TEST(parser, DISABLED_limits_min_invalid_too_short)
{
    const auto input = from_hex("00");
    EXPECT_THROW(parser<Limits>{}(input.data()), parser_error);
}

TEST(parser, DISABLED_limits_minmax_invalid_too_short)
{
    const auto input = from_hex("0120");
    EXPECT_THROW(parser<Limits>{}(input.data()), parser_error);
}

TEST(parser, limits_invalid)
{
    const auto input = from_hex("02");
    EXPECT_THROW(parser<Limits>{}(input.data()), parser_error);
}

TEST(parser, locals)
{
    const auto input = from_hex("81017f");
    const auto [l, p] = parser<Locals>{}(input.data());
    EXPECT_EQ(l.count, 0x81);
    EXPECT_EQ(l.type, ValType::i32);
}

TEST(parser, empty_module)
{
    const auto module = parse(wasm_prefix);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, module_with_wrong_prefix)
{
    EXPECT_THROW(parse({}), parser_error);
    EXPECT_THROW(parse(bytes{0x00, 0x61, 0x73, 0xd6}), parser_error);
    EXPECT_THROW(parse(bytes{0x00, 0x61, 0x73, 0xd6, 0x00, 0x00, 0x00, 0x00}), parser_error);
    EXPECT_THROW(parse(bytes{0x00, 0x61, 0x73, 0xd6, 0x02, 0x00, 0x00, 0x00}), parser_error);
}

TEST(parser, custom_section_empty)
{
    const auto bin = bytes{wasm_prefix} + from_hex("0000");
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, custom_section_nonempty)
{
    const auto bin = bytes{wasm_prefix} + from_hex("0001ff");
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, functype_wrong_prefix)
{
    const auto section_contents = uint8_t{0x01} + from_hex("610000");
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x01} + uint8_t(section_contents.size()) + section_contents;
    EXPECT_THROW(parse(bin), parser_error);
}

TEST(parser, type_section_larger_than_expected)
{
    const auto section_contents = uint8_t{0x01} + functype_void_to_void;
    const auto bin = bytes{wasm_prefix} + uint8_t{0x01} + uint8_t(section_contents.size() - 1) +
                     section_contents;
    EXPECT_THROW(parse(bin), parser_error);
}

TEST(parser, type_section_smaller_than_expected)
{
    const auto section_contents = uint8_t{0x01} + functype_void_to_void + uint8_t{0xfe};
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x01} + uint8_t(section_contents.size()) + section_contents;
    EXPECT_THROW(parse(bin), parser_error);
}

TEST(parser, type_section_with_single_functype)
{
    // single type [void] -> [void]
    const auto section_contents = uint8_t{0x01} + functype_void_to_void;
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x01} + uint8_t(section_contents.size()) + section_contents;
    const auto module = parse(bin);
    ASSERT_EQ(module.typesec.size(), 1);
    const auto functype = module.typesec[0];
    EXPECT_EQ(functype.inputs.size(), 0);
    EXPECT_EQ(functype.outputs.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, type_section_with_single_functype_params)
{
    // single type [i32, i64] -> [i32]
    const auto section_contents = uint8_t{0x01} + functype_i32i64_to_i32;
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x01} + uint8_t(section_contents.size()) + section_contents;
    const auto module = parse(bin);
    ASSERT_EQ(module.typesec.size(), 1);
    const auto functype = module.typesec[0];
    ASSERT_EQ(functype.inputs.size(), 2);
    EXPECT_EQ(functype.inputs[0], ValType::i32);
    EXPECT_EQ(functype.inputs[1], ValType::i64);
    ASSERT_EQ(functype.outputs.size(), 1);
    EXPECT_EQ(functype.outputs[0], ValType::i32);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, type_section_with_multiple_functypes)
{
    // type 0 [void] -> [void]
    // type 1 [i32, i64] -> [i32]
    // type 2 [i32] -> []
    const auto section_contents =
        uint8_t{0x03} + functype_void_to_void + functype_i32i64_to_i32 + functype_i32_to_void;
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x01} + uint8_t(section_contents.size()) + section_contents;
    const auto module = parse(bin);
    ASSERT_EQ(module.typesec.size(), 3);
    const auto functype0 = module.typesec[0];
    EXPECT_EQ(functype0.inputs.size(), 0);
    EXPECT_EQ(functype0.outputs.size(), 0);
    const auto functype1 = module.typesec[1];
    EXPECT_EQ(functype1.inputs.size(), 2);
    EXPECT_EQ(functype1.inputs[0], ValType::i32);
    EXPECT_EQ(functype1.inputs[1], ValType::i64);
    EXPECT_EQ(functype1.outputs.size(), 1);
    EXPECT_EQ(functype1.outputs[0], ValType::i32);
    const auto functype2 = module.typesec[2];
    EXPECT_EQ(functype2.inputs.size(), 1);
    EXPECT_EQ(functype2.inputs[0], ValType::i32);
    EXPECT_EQ(functype2.outputs.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, memory_single_min_limit)
{
    const auto section_contents = bytes{} + uint8_t{0x01} + uint8_t{0x00} + uint8_t{0x7f};
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x05} + uint8_t(section_contents.size()) + section_contents;

    const auto module = parse(bin);
    ASSERT_EQ(module.memorysec.size(), 1);
    EXPECT_EQ(module.memorysec[0].limits.min, 0x7f);
}

TEST(parser, memory_single_minmax_limit)
{
    const auto section_contents =
        bytes{} + uint8_t{0x01} + uint8_t{0x01} + uint8_t{0x12} + uint8_t{0x7f};
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x05} + uint8_t(section_contents.size()) + section_contents;

    const auto module = parse(bin);
    ASSERT_EQ(module.memorysec.size(), 1);
    EXPECT_EQ(module.memorysec[0].limits.min, 0x12);
    EXPECT_EQ(module.memorysec[0].limits.max, 0x7f);
}

TEST(parser, memory_multi_min_limit)
{
    const auto section_contents =
        bytes{} + uint8_t{0x02} + uint8_t{0x00} + uint8_t{0x7f} + uint8_t{0x00} + uint8_t{0x7f};
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x05} + uint8_t(section_contents.size()) + section_contents;

    const auto module = parse(bin);
    ASSERT_EQ(module.memorysec.size(), 2);
    EXPECT_EQ(module.memorysec[0].limits.min, 0x7f);
    EXPECT_EQ(module.memorysec[1].limits.min, 0x7f);
}

TEST(parser, global_single_mutable_const_inited)
{
    const auto section_contents = bytes{0x01, 0x7f, 0x01, uint8_t(Instr::i32_const), 0x10, 0x0b};
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x06} + uint8_t(section_contents.size()) + section_contents;

    const auto module = parse(bin);
    ASSERT_EQ(module.globalsec.size(), 1);
    EXPECT_TRUE(module.globalsec[0].is_mutable);
    EXPECT_EQ(module.globalsec[0].init_type, GlobalInitType::constant);
    EXPECT_EQ(module.globalsec[0].init.value, 0x10);
}

TEST(parser, global_single_const_global_inited)
{
    const auto section_contents = bytes{0x01, 0x7f, 0x00, uint8_t(Instr::global_get), 0x01, 0x0b};
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x06} + uint8_t(section_contents.size()) + section_contents;

    const auto module = parse(bin);
    ASSERT_EQ(module.globalsec.size(), 1);
    EXPECT_FALSE(module.globalsec[0].is_mutable);
    EXPECT_EQ(module.globalsec[0].init_type, GlobalInitType::global);
    EXPECT_EQ(module.globalsec[0].init.global_index, 0x01);
}

TEST(parser, global_single_multi_instructions_inited)
{
    const auto section_contents = bytes{
        0x01, 0x7f, 0x01, uint8_t(Instr::i32_const), 0x10, uint8_t(Instr::i64_const), 0x7f, 0x0b};
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x06} + uint8_t(section_contents.size()) + section_contents;

    const auto module = parse(bin);
    ASSERT_EQ(module.globalsec.size(), 1);
    EXPECT_TRUE(module.globalsec[0].is_mutable);
    EXPECT_EQ(module.globalsec[0].init_type, GlobalInitType::constant);
    EXPECT_EQ(module.globalsec[0].init.value, uint64_t(-1));
}

TEST(parser, global_multi_const_inited)
{
    const auto section_contents = bytes{0x02, 0x7f, 0x00, uint8_t(Instr::i32_const), 0x01, 0x0b,
        0x7f, 0x01, uint8_t(Instr::i32_const), 0x7f, 0x0b};
    const auto bin =
        bytes{wasm_prefix} + uint8_t{0x06} + uint8_t(section_contents.size()) + section_contents;

    const auto module = parse(bin);
    ASSERT_EQ(module.globalsec.size(), 2);
    EXPECT_FALSE(module.globalsec[0].is_mutable);
    EXPECT_EQ(module.globalsec[0].init_type, GlobalInitType::constant);
    EXPECT_EQ(module.globalsec[0].init.value, 0x01);
    EXPECT_TRUE(module.globalsec[1].is_mutable);
    EXPECT_EQ(module.globalsec[1].init_type, GlobalInitType::constant);
    EXPECT_EQ(module.globalsec[1].init.value, uint32_t(-1));
}

TEST(parser, code_with_empty_expr_2_locals)
{
    // Func with 2x i32 locals, only 0x0b "end" instruction.
    const auto func_2_locals_bin = from_hex("01027f0b");

    const auto code_bin = uint8_t(func_2_locals_bin.size()) + func_2_locals_bin;

    const auto [code_obj, end_pos1] = parser<Code>{}(code_bin.data());
    EXPECT_EQ(code_obj.local_count, 2);
    ASSERT_EQ(code_obj.instructions.size(), 1);
    EXPECT_EQ(code_obj.instructions[0], Instr::end);
    EXPECT_EQ(code_obj.immediates.size(), 0);
}

TEST(parser, code_with_empty_expr_5_locals)
{
    // Func with 1x i64 + 4x i32 locals , only 0x0b "end" instruction.
    const auto func_5_locals_bin = from_hex("02017f047e0b");

    const auto code_bin = uint8_t(func_5_locals_bin.size()) + func_5_locals_bin;

    const auto [code_obj, end_pos1] = parser<Code>{}(code_bin.data());
    EXPECT_EQ(code_obj.local_count, 5);
    ASSERT_EQ(code_obj.instructions.size(), 1);
    EXPECT_EQ(code_obj.instructions[0], Instr::end);
    EXPECT_EQ(code_obj.immediates.size(), 0);
}

TEST(parser, code_section_with_2_trivial_codes)
{
    const auto func_nolocals_bin = from_hex("000b");
    const auto code_bin = uint8_t(func_nolocals_bin.size()) + func_nolocals_bin;
    const auto section_contents = uint8_t{2} + code_bin + code_bin;
    const auto bin =
        bytes{wasm_prefix} + uint8_t{10} + uint8_t(section_contents.size()) + section_contents;
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    ASSERT_EQ(module.codesec.size(), 2);
    EXPECT_EQ(module.codesec[0].local_count, 0);
    ASSERT_EQ(module.codesec[0].instructions.size(), 1);
    EXPECT_EQ(module.codesec[0].instructions[0], Instr::end);
    EXPECT_EQ(module.codesec[1].local_count, 0);
    ASSERT_EQ(module.codesec[1].instructions.size(), 1);
    EXPECT_EQ(module.codesec[1].instructions[0], Instr::end);
}

TEST(parser, code_section_with_basic_instructions)
{
    const auto func_bin = from_hex(
        "00"  // vec(locals)
        "2001210222036a01000b");
    const auto code_bin = uint8_t(func_bin.size()) + func_bin;
    const auto section_contents = uint8_t{1} + code_bin;
    const auto bin =
        bytes{wasm_prefix} + uint8_t{10} + uint8_t(section_contents.size()) + section_contents;
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    ASSERT_EQ(module.codesec.size(), 1);
    EXPECT_EQ(module.codesec[0].local_count, 0);
    ASSERT_EQ(module.codesec[0].instructions.size(), 7);
    EXPECT_EQ(module.codesec[0].instructions[0], Instr::local_get);
    EXPECT_EQ(module.codesec[0].instructions[1], Instr::local_set);
    EXPECT_EQ(module.codesec[0].instructions[2], Instr::local_tee);
    EXPECT_EQ(module.codesec[0].instructions[3], Instr::i32_add);
    EXPECT_EQ(module.codesec[0].instructions[4], Instr::nop);
    EXPECT_EQ(module.codesec[0].instructions[5], Instr::unreachable);
    EXPECT_EQ(module.codesec[0].instructions[6], Instr::end);
    ASSERT_EQ(module.codesec[0].immediates.size(), 3 * 4);
    EXPECT_EQ(module.codesec[0].immediates, from_hex("010000000200000003000000"));
}

TEST(parser, milestone1)
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

    const auto bin = from_hex(
        "0061736d0100000001070160027f7f017f030201000a13011101017f200020016a20026a220220006a0b");
    const auto m = parse(bin);

    ASSERT_EQ(m.typesec.size(), 1);
    EXPECT_EQ(m.typesec[0].inputs, (std::vector{ValType::i32, ValType::i32}));
    EXPECT_EQ(m.typesec[0].outputs, (std::vector{ValType::i32}));

    ASSERT_EQ(m.codesec.size(), 1);
    const auto& c = m.codesec[0];
    EXPECT_EQ(c.local_count, 1);
    EXPECT_EQ(c.instructions,
        (std::vector{Instr::local_get, Instr::local_get, Instr::i32_add, Instr::local_get,
            Instr::i32_add, Instr::local_tee, Instr::local_get, Instr::i32_add, Instr::end}));
    EXPECT_EQ(c.immediates, from_hex("00000000"
                                     "01000000"
                                     "02000000"
                                     "02000000"
                                     "00000000"));
}