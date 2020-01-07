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
    EXPECT_EQ(std::get<0>(parser<valtype>{}(&b)), valtype::i64);
    b = 0x7f;
    EXPECT_EQ(std::get<0>(parser<valtype>{}(&b)), valtype::i32);
    b = 0x7d;
    EXPECT_THROW(std::get<0>(parser<valtype>{}(&b)), parser_error);
}

TEST(parser, valtype_vec)
{
    const auto input = from_hex("037f7e7fcc");
    const auto [vec, pos] = parser<std::vector<valtype>>{}(input.data());
    EXPECT_EQ(pos, input.data() + 4);
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], valtype::i32);
    EXPECT_EQ(vec[1], valtype::i64);
    EXPECT_EQ(vec[2], valtype::i32);
}

TEST(parser, locals)
{
    const auto input = from_hex("81017f");
    const auto [l, p] = parser<locals>{}(input.data());
    EXPECT_EQ(l.count, 0x81);
    EXPECT_EQ(l.type, valtype::i32);
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
    EXPECT_EQ(functype.inputs[0], valtype::i32);
    EXPECT_EQ(functype.inputs[1], valtype::i64);
    ASSERT_EQ(functype.outputs.size(), 1);
    EXPECT_EQ(functype.outputs[0], valtype::i32);
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
    EXPECT_EQ(functype1.inputs[0], valtype::i32);
    EXPECT_EQ(functype1.inputs[1], valtype::i64);
    EXPECT_EQ(functype1.outputs.size(), 1);
    EXPECT_EQ(functype1.outputs[0], valtype::i32);
    const auto functype2 = module.typesec[2];
    EXPECT_EQ(functype2.inputs.size(), 1);
    EXPECT_EQ(functype2.inputs[0], valtype::i32);
    EXPECT_EQ(functype2.outputs.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}
