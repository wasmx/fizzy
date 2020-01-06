#include "parser.hpp"
#include "utils.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

static const auto wasm_prefix = from_hex("006173d601000000");

TEST(parser, empty_module)
{
    const auto module = parse(wasm_prefix);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, custom_section_empty)
{
    const auto bin = wasm_prefix + from_hex("0000");
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, custom_section_nonempty)
{
    const auto bin = wasm_prefix + from_hex("0001ff");
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, type_section_with_single_functype)
{
    const auto bin = wasm_prefix + from_hex("01020000");
    const auto module = parse(bin);
    ASSERT_EQ(module.typesec.size(), 1);
    const auto functype = module.typesec[0];
    EXPECT_EQ(functype.inputs.size(), 0);
    EXPECT_EQ(functype.outputs.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}
