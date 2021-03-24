// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "instructions.hpp"
#include "parser.hpp"
#include <gmock/gmock.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>
#include <test/utils/leb128_encode.hpp>
#include <test/utils/wasm_binary.hpp>

using namespace fizzy;
using namespace fizzy::test;
using namespace testing;

namespace
{
constexpr uint8_t i32 = 0x7f;
constexpr uint8_t i64 = 0x7e;

bytes make_invalid_size_section(uint8_t id, size_t size, const bytes& content)
{
    return bytes{id} + test::leb128u_encode(size) + content;
}

bytes make_functype(bytes input_types, bytes output_types)
{
    // make_vec() not used here, because it requires different argument type.
    return bytes{0x60} + test::leb128u_encode(input_types.size()) + input_types +
           test::leb128u_encode(output_types.size()) + output_types;
}
}  // namespace

TEST(parser, valtype)
{
    auto wasm_bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {0xcc})}));
    auto& valtype_bin = wasm_bin.back();

    valtype_bin = 0x7e;
    EXPECT_EQ(parse(wasm_bin)->typesec.front().outputs.front(), ValType::i64);
    valtype_bin = 0x7f;
    EXPECT_EQ(parse(wasm_bin)->typesec.front().outputs.front(), ValType::i32);
    valtype_bin = 0x7c;
    EXPECT_EQ(parse(wasm_bin)->typesec.front().outputs.front(), ValType::f64);
    valtype_bin = 0x7d;
    EXPECT_EQ(parse(wasm_bin)->typesec.front().outputs.front(), ValType::f32);
    valtype_bin = 0x7a;
    EXPECT_THROW_MESSAGE(parse(wasm_bin), parser_error, "invalid valtype 122");
}

TEST(parser, vec_malformend_huge_size)
{
    // Malformed vec as size only without any elements:
    const auto vec_bin = test::leb128u_encode(std::numeric_limits<uint32_t>::max());
    const auto wasm_bin = bytes{wasm_prefix} + make_section(1, vec_bin);
    EXPECT_THROW_MESSAGE(parse(wasm_bin), parser_error, "unexpected EOF");
}

TEST(parser, limits_min)
{
    const auto wasm = bytes{wasm_prefix} + make_section(5, make_vec({"007f"_bytes}));
    const auto module = parse(wasm);
    const auto& limits = module->memorysec[0].limits;
    EXPECT_EQ(limits.min, 0x7f);
    EXPECT_FALSE(limits.max.has_value());
}

TEST(parser, limits_minmax)
{
    const auto wasm = bytes{wasm_prefix} + make_section(5, make_vec({"01207f"_bytes}));
    const auto module = parse(wasm);
    const auto& limits = module->memorysec[0].limits;
    EXPECT_EQ(limits.min, 0x20);
    EXPECT_TRUE(limits.max.has_value());
    EXPECT_EQ(*limits.max, 0x7f);
}

TEST(parser, limits_min_invalid_too_short)
{
    const auto wasm = bytes{wasm_prefix} + make_section(5, make_vec({"00"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, limits_minmax_invalid_too_short)
{
    const auto wasm = bytes{wasm_prefix} + make_section(5, make_vec({"0120"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, limits_invalid)
{
    const auto wasm = bytes{wasm_prefix} + make_section(5, make_vec({"02"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "invalid limits 2");
}

TEST(parser, module_empty)
{
    const auto module = parse(wasm_prefix);
    EXPECT_EQ(module->typesec.size(), 0);
    EXPECT_EQ(module->funcsec.size(), 0);
    EXPECT_EQ(module->codesec.size(), 0);
}

TEST(parser, module_with_wrong_prefix)
{
    EXPECT_THROW_CODE_MESSAGE(parse({}), parser_error, FIZZY_ERROR_PARSER_INVALID_MODULE_PREFIX,
        "invalid wasm module prefix");
    EXPECT_THROW_MESSAGE(parse("006173d6"_bytes), parser_error, "invalid wasm module prefix");
    EXPECT_THROW_MESSAGE(
        parse("006173d600000000"_bytes), parser_error, "invalid wasm module prefix");
    EXPECT_THROW_MESSAGE(
        parse("006173d602000000"_bytes), parser_error, "invalid wasm module prefix");
}

TEST(parser, section_vec_size_out_of_bounds)
{
    const auto malformed_vec_size = "81"_bytes;
    for (auto secid : {1, 2, 3, 4, 5, 6, 7, 9, 10, 11})
    {
        const auto wasm = bytes{wasm_prefix} + make_section(uint8_t(secid), malformed_vec_size);
        EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
    }
}

TEST(parser, custom_section_empty)
{
    // Section consists of an empty name
    const auto bin = bytes{wasm_prefix} + make_section(0, "00"_bytes);
    const auto module = parse(bin);
    EXPECT_EQ(module->typesec.size(), 0);
    EXPECT_EQ(module->funcsec.size(), 0);
    EXPECT_EQ(module->codesec.size(), 0);
}

TEST(parser, custom_section_nonempty_name_only)
{
    // Section consists of only the name "abc"
    const auto bin = bytes{wasm_prefix} + make_section(0, "03616263"_bytes);
    const auto module = parse(bin);
    EXPECT_EQ(module->typesec.size(), 0);
    EXPECT_EQ(module->funcsec.size(), 0);
    EXPECT_EQ(module->codesec.size(), 0);
}

TEST(parser, custom_section_name_not_ascii)
{
    // Section consists of only the name with some non-ascii characters.
    const auto bin = bytes{wasm_prefix} + make_section(0, "0670617765c582"_bytes);
    const auto module = parse(bin);
    EXPECT_EQ(module->typesec.size(), 0);
    EXPECT_EQ(module->funcsec.size(), 0);
    EXPECT_EQ(module->codesec.size(), 0);
}

TEST(parser, custom_section_nonempty)
{
    // Section consists of the name "abc" and 14 bytes of unparsed data
    const auto bin =
        bytes{wasm_prefix} + make_section(0, "036162630000112233445566778899000099"_bytes);
    const auto module = parse(bin);
    EXPECT_EQ(module->typesec.size(), 0);
    EXPECT_EQ(module->funcsec.size(), 0);
    EXPECT_EQ(module->codesec.size(), 0);
}

TEST(parser, custom_section_size_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + "0080"_bytes;
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, custom_section_name_out_of_bounds)
{
    const auto bin = bytes{wasm_prefix} + make_section(0, "01"_bytes);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "unexpected EOF");
}

TEST(parser, custom_section_name_exceeds_section_size)
{
    const auto bin = bytes{wasm_prefix} + make_invalid_size_section(0, 1, "01aa"_bytes) +
                     make_section(0, "00"_bytes);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "unexpected EOF");
}

TEST(parser, custom_section_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_invalid_size_section(0, 31, {});
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, custom_section_invalid_utf8)
{
    const auto bin = bytes{wasm_prefix} + make_section(0, "027f80"_bytes);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "invalid UTF-8");
}

TEST(parser, type_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(1, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->typesec.size(), 0);
}

TEST(parser, type_section_wrong_prefix)
{
    const auto section_contents =
        "01"
        "610000"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(1, section_contents);
    EXPECT_THROW_MESSAGE(
        parse(bin), parser_error, "unexpected byte value 97, expected 0x60 for functype");
}

TEST(parser, type_section_larger_than_expected)
{
    const auto section_contents = "01"_bytes + make_functype({}, {});
    const auto bin =
        bytes{wasm_prefix} +
        make_invalid_size_section(1, size_t{section_contents.size() - 1}, section_contents);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "incorrect section 1 size, difference: 1");
}

TEST(parser, type_section_smaller_than_expected)
{
    const auto section_contents = make_vec({make_functype({}, {})}) + "fe"_bytes;
    const auto bin =
        bytes{wasm_prefix} +
        make_invalid_size_section(1, size_t{section_contents.size() + 1}, section_contents) +
        "00"_bytes;
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "incorrect section 1 size, difference: -2");
}

TEST(parser, type_section_with_single_functype)
{
    // single type [void] -> [void]
    const auto section_contents = make_vec({make_functype({}, {})});
    const auto bin = bytes{wasm_prefix} + make_section(1, section_contents);
    const auto module = parse(bin);
    ASSERT_EQ(module->typesec.size(), 1);
    const auto functype = module->typesec[0];
    EXPECT_EQ(functype.inputs.size(), 0);
    EXPECT_EQ(functype.outputs.size(), 0);
    EXPECT_EQ(module->funcsec.size(), 0);
    EXPECT_EQ(module->codesec.size(), 0);
}

TEST(parser, type_section_with_single_functype_params)
{
    // single type [i32, i64, i32] -> [i32]
    const auto section_contents = make_vec({make_functype({i32, i64, i32}, {i32})});
    const auto bin = bytes{wasm_prefix} + make_section(1, section_contents);
    const auto module = parse(bin);
    ASSERT_EQ(module->typesec.size(), 1);
    const auto functype = module->typesec[0];
    ASSERT_EQ(functype.inputs.size(), 3);
    EXPECT_EQ(functype.inputs[0], ValType::i32);
    EXPECT_EQ(functype.inputs[1], ValType::i64);
    EXPECT_EQ(functype.inputs[2], ValType::i32);
    ASSERT_EQ(functype.outputs.size(), 1);
    EXPECT_EQ(functype.outputs[0], ValType::i32);
    EXPECT_EQ(module->funcsec.size(), 0);
    EXPECT_EQ(module->codesec.size(), 0);
}

TEST(parser, type_section_with_multiple_functypes)
{
    // type 0 [void] -> [void]
    // type 1 [i32, i64] -> [i32]
    // type 2 [i32] -> []
    const auto section_contents = make_vec(
        {make_functype({}, {}), make_functype({i32, i64}, {i32}), make_functype({i32}, {})});
    const auto bin = bytes{wasm_prefix} + make_section(1, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module->typesec.size(), 3);
    const auto functype0 = module->typesec[0];
    EXPECT_EQ(functype0.inputs.size(), 0);
    EXPECT_EQ(functype0.outputs.size(), 0);
    const auto functype1 = module->typesec[1];
    EXPECT_EQ(functype1.inputs.size(), 2);
    EXPECT_EQ(functype1.inputs[0], ValType::i32);
    EXPECT_EQ(functype1.inputs[1], ValType::i64);
    EXPECT_EQ(functype1.outputs.size(), 1);
    EXPECT_EQ(functype1.outputs[0], ValType::i32);
    const auto functype2 = module->typesec[2];
    EXPECT_EQ(functype2.inputs.size(), 1);
    EXPECT_EQ(functype2.inputs[0], ValType::i32);
    EXPECT_EQ(functype2.outputs.size(), 0);
    EXPECT_EQ(module->funcsec.size(), 0);
    EXPECT_EQ(module->codesec.size(), 0);
}

TEST(parser, type_section_functype_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(1, make_vec({""_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, import_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(2, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->importsec.size(), 0);
}

TEST(parser, import_single_function)
{
    const auto type_section = make_vec({make_functype({}, {}), make_functype({0x7f}, {})});
    const auto import_section = bytes{0x01, 0x03, 'm', 'o', 'd', 0x03, 'f', 'o', 'o', 0x00, 0x01};
    const auto bin =
        bytes{wasm_prefix} + make_section(1, type_section) + make_section(2, import_section);

    const auto module = parse(bin);
    ASSERT_EQ(module->importsec.size(), 1);
    EXPECT_EQ(module->importsec[0].module, "mod");
    EXPECT_EQ(module->importsec[0].name, "foo");
    EXPECT_EQ(module->importsec[0].kind, ExternalKind::Function);
    EXPECT_EQ(module->importsec[0].desc.function_type_index, 1);
}

TEST(parser, import_multiple)
{
    const auto type_section = make_vec({make_functype({}, {}), make_functype({0x7f}, {})});
    const auto import_section = make_vec({bytes{0x02, 'm', '1', 0x03, 'a', 'b', 'c', 0x00, 0x01},
        bytes{0x02, 'm', '2', 0x03, 'f', 'o', 'o', 0x02, 0x00, 0x7f},
        bytes{0x02, 'm', '3', 0x03, 'b', 'a', 'r', 0x03, 0x7f, 0x00},
        bytes{0x02, 'm', '4', 0x03, 't', 'a', 'b', 0x01, 0x70, 0x01, 0x01, 0x42}});
    const auto bin =
        bytes{wasm_prefix} + make_section(1, type_section) + make_section(2, import_section);

    const auto module = parse(bin);
    ASSERT_EQ(module->importsec.size(), 4);
    EXPECT_EQ(module->importsec[0].module, "m1");
    EXPECT_EQ(module->importsec[0].name, "abc");
    EXPECT_EQ(module->importsec[0].kind, ExternalKind::Function);
    EXPECT_EQ(module->importsec[0].desc.function_type_index, 0x01);
    EXPECT_EQ(module->importsec[1].module, "m2");
    EXPECT_EQ(module->importsec[1].name, "foo");
    EXPECT_EQ(module->importsec[1].kind, ExternalKind::Memory);
    EXPECT_EQ(module->importsec[1].desc.memory.limits.min, 0x7f);
    EXPECT_FALSE(module->importsec[1].desc.memory.limits.max);
    EXPECT_EQ(module->importsec[2].module, "m3");
    EXPECT_EQ(module->importsec[2].name, "bar");
    EXPECT_EQ(module->importsec[2].kind, ExternalKind::Global);
    EXPECT_FALSE(module->importsec[2].desc.global.is_mutable);
    EXPECT_EQ(module->importsec[2].desc.global.value_type, ValType::i32);
    EXPECT_EQ(module->importsec[3].module, "m4");
    EXPECT_EQ(module->importsec[3].name, "tab");
    EXPECT_EQ(module->importsec[3].kind, ExternalKind::Table);
    EXPECT_EQ(module->importsec[3].desc.table.limits.min, 1);
    ASSERT_TRUE(module->importsec[3].desc.table.limits.max.has_value());
    EXPECT_EQ(*module->importsec[3].desc.table.limits.max, 0x42);
}

TEST(parser, import_invalid_kind)
{
    const auto wasm = bytes{wasm_prefix} + make_section(2, make_vec({"000004"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected import kind value 4");
}

TEST(parser, import_kind_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(2, make_vec({"0000"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, import_invalid_utf8_in_module)
{
    const auto section_contents =
        bytes{0x01, 0x03, 'm', 0x80, 'd', 0x03, 'f', 'o', 'o', 0x00, 0x42};
    const auto wasm = bytes{wasm_prefix} + make_section(2, section_contents);
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "invalid UTF-8");
}

TEST(parser, import_invalid_utf8_in_name)
{
    const auto section_contents =
        bytes{0x01, 0x03, 'm', 'o', 'd', 0x03, 'f', 0x80, 'o', 0x00, 0x42};
    const auto wasm = bytes{wasm_prefix} + make_section(2, section_contents);
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "invalid UTF-8");
}

TEST(parser, function_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(3, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->funcsec.size(), 0);
}

TEST(parser, function_section_with_single_function)
{
    const auto type_section = make_vec({make_functype({}, {})});
    const auto function_section = make_vec({"00"_bytes});
    const auto code_section = make_vec({"02000b"_bytes});
    const auto bin = bytes{wasm_prefix} + make_section(1, type_section) +
                     make_section(3, function_section) + make_section(10, code_section);
    const auto module = parse(bin);
    ASSERT_EQ(module->funcsec.size(), 1);
    EXPECT_EQ(module->funcsec[0], 0);
}

TEST(parser, function_section_with_multiple_functions)
{
    const auto type_section = make_vec({make_functype({}, {}), make_functype({}, {}),
        make_functype({}, {}), make_functype({0x7f}, {}), make_functype({}, {})});

    const auto function_section = "0400010304"_bytes;
    const auto code_section =
        make_vec({"02000b"_bytes, "02000b"_bytes, "02000b"_bytes, "02000b"_bytes});
    const auto bin = bytes{wasm_prefix} + make_section(1, type_section) +
                     make_section(3, function_section) + make_section(10, code_section);
    const auto module = parse(bin);
    ASSERT_EQ(module->funcsec.size(), 4);
    EXPECT_EQ(module->funcsec[0], 0);
    EXPECT_EQ(module->funcsec[1], 1);
    EXPECT_EQ(module->funcsec[2], 3);
    EXPECT_EQ(module->funcsec[3], 4);
}

TEST(parser, function_section_size_128)
{
    constexpr auto size = 128;
    const auto type_section = make_vec({make_functype({}, {})});
    const auto function_section = test::leb128u_encode(size) + bytes(128, 0);
    bytes code_section = test::leb128u_encode(size);
    for (auto i = 0; i < size; i++)
        code_section.append("02000b"_bytes);
    const auto wasm_bin = bytes{wasm_prefix} + make_section(1, type_section) +
                          make_section(3, function_section) + make_section(10, code_section);
    const auto module = parse(wasm_bin);
    ASSERT_EQ(module->funcsec.size(), size);
}

TEST(parser, function_section_end_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_invalid_size_section(3, 2, {});
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, function_section_without_code)
{
    const auto type_section = make_vec({make_functype({}, {})});
    const auto function_section = "0100"_bytes;
    const auto bin =
        bytes{wasm_prefix} + make_section(1, type_section) + make_section(3, function_section);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error,
        "malformed binary: number of function and code entries must match");
}

TEST(parser, function_section_invalid_type_index)
{
    const auto type_section = make_vec({make_functype({}, {})});
    const auto function_section0 = make_vec({bytes{0x00}});
    const auto function_section1 = make_vec({bytes{0x01}});
    const auto code_section = make_vec({"02000b"_bytes});

    const auto wasm0 =
        bytes{wasm_prefix} + make_section(3, function_section0) + make_section(10, code_section);
    EXPECT_THROW_MESSAGE(parse(wasm0), validation_error, "invalid function type index");

    const auto wasm1 = bytes{wasm_prefix} + make_section(1, type_section) +
                       make_section(3, function_section1) + make_section(10, code_section);
    EXPECT_THROW_MESSAGE(parse(wasm1), validation_error, "invalid function type index");
}

TEST(parser, table_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(4, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->tablesec.size(), 0);
}

TEST(parser, table_single_min_limit)
{
    const auto section_contents = "0170007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module->tablesec.size(), 1);
    EXPECT_EQ(module->tablesec[0].limits.min, 0x7f);
}

TEST(parser, table_single_minmax_limit)
{
    const auto section_contents = "017001127f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module->tablesec.size(), 1);
    EXPECT_EQ(module->tablesec[0].limits.min, 0x12);
    EXPECT_EQ(module->tablesec[0].limits.max, 0x7f);
}

// Where minimum exceeds maximum
TEST(parser, table_single_malformed_minmax)
{
    const auto section_contents = "0170017f12"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "malformed limits (minimum is larger than maximum)");
}

TEST(parser, table_invalid_elemtype)
{
    const auto wasm = bytes{wasm_prefix} + make_section(4, make_vec({"71"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected table elemtype: 113");
}

TEST(parser, table_elemtype_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(4, make_vec({""_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, memory_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(5, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->memorysec.size(), 0);
}

TEST(parser, memory_single_min_limit)
{
    const auto section_contents = "01007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module->memorysec.size(), 1);
    EXPECT_EQ(module->memorysec[0].limits.min, 0x7f);
}

TEST(parser, memory_single_minmax_limit)
{
    const auto section_contents = "0101127f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module->memorysec.size(), 1);
    EXPECT_EQ(module->memorysec[0].limits.min, 0x12);
    EXPECT_EQ(module->memorysec[0].limits.max, 0x7f);
}

// Where minimum exceeds maximum
TEST(parser, memory_single_malformed_minmax)
{
    const auto section_contents = "01017f12"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "malformed limits (minimum is larger than maximum)");
}

TEST(parser, memory_single_invalid_min_limit)
{
    // min=65537
    const auto section_contents = "0100818004"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    EXPECT_THROW_MESSAGE(parse(bin), validation_error, "maximum memory page limit exceeded");
}

TEST(parser, memory_single_invalid_max_limit)
{
    // min=1 max=65537
    const auto section_contents = "010101818004"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    EXPECT_THROW_MESSAGE(parse(bin), validation_error, "maximum memory page limit exceeded");
}

TEST(parser, memory_limits_kind_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(5, make_vec({""_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, global_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(6, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->globalsec.size(), 0);
}

TEST(parser, global_single_mutable_const_inited)
{
    const auto section_contents = bytes{0x01, 0x7f, 0x01, uint8_t(Instr::i32_const), 0x10, 0x0b};
    const auto bin = bytes{wasm_prefix} + make_section(6, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module->globalsec.size(), 1);
    EXPECT_TRUE(module->globalsec[0].type.is_mutable);
    EXPECT_EQ(module->globalsec[0].type.value_type, ValType::i32);
    EXPECT_EQ(module->globalsec[0].expression.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module->globalsec[0].expression.value.constant.i32, 0x10);
}

TEST(parser, global_multi_global_inited)
{
    /* wat2wasm
      (global (import "m" "g1") i32)
      (global (import "m" "g2") i32)
      (global i32 (global.get 1))
    */
    const auto bin =
        from_hex("0061736d01000000021102016d026731037f00016d026732037f000606017f0023010b");
    const auto module = parse(bin);

    ASSERT_EQ(module->globalsec.size(), 1);
    EXPECT_FALSE(module->globalsec[0].type.is_mutable);
    EXPECT_EQ(module->globalsec[0].type.value_type, ValType::i32);
    EXPECT_EQ(module->globalsec[0].expression.kind, ConstantExpression::Kind::GlobalGet);
    EXPECT_EQ(module->globalsec[0].expression.value.global_index, 0x01);
}

TEST(parser, global_multi_const_inited)
{
    const auto section_contents =
        make_vec({bytes{0x7f, 0x00, uint8_t(Instr::i32_const), 0x01, 0x0b},
            bytes{0x7f, 0x01, uint8_t(Instr::i32_const), 0x7f, 0x0b}});
    const auto bin = bytes{wasm_prefix} + make_section(6, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module->globalsec.size(), 2);
    EXPECT_FALSE(module->globalsec[0].type.is_mutable);
    EXPECT_EQ(module->globalsec[0].type.value_type, ValType::i32);
    EXPECT_EQ(module->globalsec[0].expression.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module->globalsec[0].expression.value.constant.i32, 1);
    EXPECT_TRUE(module->globalsec[1].type.is_mutable);
    EXPECT_EQ(module->globalsec[1].type.value_type, ValType::i32);
    EXPECT_EQ(module->globalsec[1].expression.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module->globalsec[1].expression.value.constant.i32, -1);
}

TEST(parser, global_float)
{
    /* wat2wasm
      (global (import "m" "g1") (mut f32))
      (global (import "m" "g2") f64)
      (global f32 (f32.const 1.2))
      (global (mut f64) (f64.const 3.4))
      (global f64 (global.get 1))
    */
    const auto bin = from_hex(
        "0061736d01000000021102016d026731037d01016d026732037c00061a037d00439a99993f0b7c014433333333"
        "33330b400b7c0023010b");

    const auto module = parse(bin);
    ASSERT_EQ(module->importsec.size(), 2);
    EXPECT_EQ(module->importsec[0].kind, ExternalKind::Global);
    EXPECT_EQ(module->importsec[0].module, "m");
    EXPECT_EQ(module->importsec[0].name, "g1");
    EXPECT_TRUE(module->importsec[0].desc.global.is_mutable);
    EXPECT_EQ(module->importsec[0].desc.global.value_type, ValType::f32);
    EXPECT_EQ(module->importsec[1].kind, ExternalKind::Global);
    EXPECT_EQ(module->importsec[1].module, "m");
    EXPECT_EQ(module->importsec[1].name, "g2");
    EXPECT_FALSE(module->importsec[1].desc.global.is_mutable);
    EXPECT_EQ(module->importsec[1].desc.global.value_type, ValType::f64);

    ASSERT_EQ(module->globalsec.size(), 3);
    EXPECT_FALSE(module->globalsec[0].type.is_mutable);
    EXPECT_EQ(module->globalsec[0].type.value_type, ValType::f32);
    EXPECT_EQ(module->globalsec[0].expression.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module->globalsec[0].expression.value.constant.f32, 1.2f);
    EXPECT_TRUE(module->globalsec[1].type.is_mutable);
    EXPECT_EQ(module->globalsec[1].type.value_type, ValType::f64);
    EXPECT_EQ(module->globalsec[1].expression.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module->globalsec[1].expression.value.constant.f64, 3.4);
    EXPECT_FALSE(module->globalsec[2].type.is_mutable);
    EXPECT_EQ(module->globalsec[2].type.value_type, ValType::f64);
    EXPECT_EQ(module->globalsec[2].expression.kind, ConstantExpression::Kind::GlobalGet);
    EXPECT_EQ(module->globalsec[2].expression.value.global_index, 1);
}

TEST(parser, global_invalid_mutability)
{
    const auto wasm = bytes{wasm_prefix} + make_section(6, make_vec({"7f02"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error,
        "unexpected byte value 2, expected 0x00 or 0x01 for global mutability");
}

TEST(parser, global_invalid_initializer)
{
    // valtype=i32 mutability=0 expression=<memory_size>
    const auto wasm = bytes{wasm_prefix} + make_section(6, make_vec({"7f003f"_bytes}));
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "unexpected instruction in the constant expression: 63");
}

TEST(parser, global_valtype_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(6, make_vec({""_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, global_mutability_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(6, make_vec({"7f"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, global_constant_expression_out_of_bounds)
{
    // i32, immutable, EOF.
    const auto wasm1 = bytes{wasm_prefix} + make_section(6, make_vec({"7f00"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm1), parser_error, "unexpected EOF");

    // i32, immutable, i32_const, 0, EOF.
    const auto wasm2 = bytes{wasm_prefix} + make_section(6, make_vec({"7f004100"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm2), parser_error, "unexpected EOF");

    // i32, immutable, i32_const, 0x81, EOF.
    const auto wasm3 = bytes{wasm_prefix} + make_section(6, make_vec({"7f004181"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm3), parser_error, "unexpected EOF");

    // i32, immutable, i64_const, 0x808081, EOF.
    const auto wasm4 = bytes{wasm_prefix} + make_section(6, make_vec({"7f0042808081"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm4), parser_error, "unexpected EOF");

    // i32, immutable, f32_const, 0xaabbcc, EOF.
    const auto wasm5 = bytes{wasm_prefix} + make_section(6, make_vec({"7f0043aabbcc"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm5), parser_error, "unexpected EOF");

    // i32, immutable, f64_const, 0xaabbccddee, EOF.
    const auto wasm6 = bytes{wasm_prefix} + make_section(6, make_vec({"7f0044aabbccddee"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm6), parser_error, "unexpected EOF");
}

TEST(parser, export_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(7, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->exportsec.size(), 0);
}

TEST(parser, export_single_function)
{
    const auto type_section = make_vec({make_functype({}, {})});
    const auto func_section = make_vec({"00"_bytes, "00"_bytes});
    const auto code_section = make_vec({"02000b"_bytes, "02000b"_bytes});

    const auto section_contents = make_vec({bytes{0x03, 'a', 'b', 'c', 0, 0}});
    const auto bin = bytes{wasm_prefix} + make_section(1, type_section) +
                     make_section(3, func_section) + make_section(7, section_contents) +
                     make_section(10, code_section);

    const auto module = parse(bin);
    ASSERT_EQ(module->exportsec.size(), 1);
    EXPECT_EQ(module->exportsec[0].name, "abc");
    EXPECT_EQ(module->exportsec[0].kind, ExternalKind::Function);
    EXPECT_EQ(module->exportsec[0].index, 0);
}

TEST(parser, export_multiple)
{
    /* wat2wasm
      (func (export "abc") nop)
      (table (export "foo") 1 funcref)
      (memory (export "bar") 1)
      (global (export "xyz") i32 (i32.const 0))
    */
    const auto wasm = from_hex(
        "0061736d010000000104016000000302010004040170000105030100010606017f0041000b0719040361626300"
        "0003666f6f01000362617202000378797a03000a05010300010b");

    const auto module = parse(wasm);
    ASSERT_EQ(module->exportsec.size(), 4);
    EXPECT_EQ(module->exportsec[0].name, "abc");
    EXPECT_EQ(module->exportsec[0].kind, ExternalKind::Function);
    EXPECT_EQ(module->exportsec[0].index, 0);
    EXPECT_EQ(module->exportsec[1].name, "foo");
    EXPECT_EQ(module->exportsec[1].kind, ExternalKind::Table);
    EXPECT_EQ(module->exportsec[1].index, 0);
    EXPECT_EQ(module->exportsec[2].name, "bar");
    EXPECT_EQ(module->exportsec[2].kind, ExternalKind::Memory);
    EXPECT_EQ(module->exportsec[2].index, 0);
    EXPECT_EQ(module->exportsec[3].name, "xyz");
    EXPECT_EQ(module->exportsec[3].kind, ExternalKind::Global);
    EXPECT_EQ(module->exportsec[3].index, 0);
}

TEST(parser, export_invalid_kind)
{
    const auto wasm = bytes{wasm_prefix} + make_section(7, make_vec({"0004"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected export kind value 4");
}

TEST(parser, export_kind_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(7, make_vec({"00"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, export_invalid_utf8)
{
    const auto section_contents = make_vec({bytes{0x03, 'a', 0x80, 'c', 0x00, 0x42}});
    const auto wasm = bytes{wasm_prefix} + make_section(7, section_contents);
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "invalid UTF-8");
}

TEST(parser, export_name_out_of_bounds)
{
    const auto wasm1 = bytes{wasm_prefix} + make_section(7, make_vec({"01"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm1), parser_error, "unexpected EOF");

    const auto wasm2 = bytes{wasm_prefix} + make_section(7, make_vec({"7faabbccddeeff"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm2), parser_error, "unexpected EOF");
}

TEST(parser, start)
{
    const auto type_section = make_vec({make_functype({}, {})});
    const auto func_section = make_vec({"00"_bytes, "00"_bytes});
    const auto code_section = make_vec({"02000b"_bytes, "02000b"_bytes});
    const auto start_section = "01"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(1, type_section) +
                     make_section(3, func_section) + make_section(8, start_section) +
                     make_section(10, code_section);

    const auto module = parse(bin);
    EXPECT_TRUE(module->startfunc);
    EXPECT_EQ(*module->startfunc, 1);
}

TEST(parser, start_invalid_index)
{
    const auto type_section = make_vec({make_functype({}, {})});
    const auto func_section = make_vec({"00"_bytes, "00"_bytes});
    const auto code_section = make_vec({"02000b"_bytes, "02000b"_bytes});
    const auto start_section = "02"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(1, type_section) +
                     make_section(3, func_section) + make_section(8, start_section) +
                     make_section(10, code_section);

    EXPECT_THROW_MESSAGE(parse(bin), validation_error, "invalid start function index");
}

TEST(parser, start_missing_funcsec)
{
    const auto start_section = "01"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(8, start_section);

    EXPECT_THROW_MESSAGE(parse(bin), validation_error, "invalid start function index");
}

TEST(parser, start_module_with_imports)
{
    const auto type_section = make_vec({make_functype({}, {})});
    const auto import_section =
        make_vec({bytes{0x03, 'm', 'o', 'd', 0x03, 'f', 'o', 'o', 0x00, 0x00}});
    const auto func_section = make_vec({"00"_bytes, "00"_bytes});
    const auto code_section = make_vec({"02000b"_bytes, "02000b"_bytes});
    const auto start_section = "02"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(1, type_section) +
                     make_section(2, import_section) + make_section(3, func_section) +
                     make_section(8, start_section) + make_section(10, code_section);

    const auto module = parse(bin);
    EXPECT_TRUE(module->startfunc);
    EXPECT_EQ(*module->startfunc, 2);
}

TEST(parser, start_module_with_imports_invalid_index)
{
    const auto type_section = make_vec({make_functype({}, {})});
    const auto import_section =
        make_vec({bytes{0x03, 'm', 'o', 'd', 0x03, 'f', 'o', 'o', 0x00, 0x00}});
    const auto func_section = make_vec({"00"_bytes, "00"_bytes});
    const auto code_section = make_vec({"02000b"_bytes, "02000b"_bytes});
    const auto start_section = "03"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(1, type_section) +
                     make_section(2, import_section) + make_section(3, func_section) +
                     make_section(8, start_section) + make_section(10, code_section);

    EXPECT_THROW_MESSAGE(parse(bin), validation_error, "invalid start function index");
}

TEST(parser, start_index_decode_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(8, "ff"_bytes);
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected EOF");
}

TEST(parser, element_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(9, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->elementsec.size(), 0);
}

TEST(parser, element_section)
{
    /* wat2wasm
      (global (import "m" "g") i32)
      (table 0 10 funcref)
      (elem (i32.const 1) 0 1)
      (elem (i32.const 2) 2 3)
      (elem (global.get 0) 0 3)
      (func)
      (func)
      (func)
      (func)
    */
    const auto bin = from_hex(
        "0061736d01000000010401600000020801016d0167037f00030504000000000405017001000a0916030041010b"
        "0200010041020b0202030023000b0200030a0d0402000b02000b02000b02000b");
    const auto module = parse(bin);

    ASSERT_EQ(module->elementsec.size(), 3);
    EXPECT_EQ(module->elementsec[0].offset.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module->elementsec[0].offset.value.constant.i32, 1);
    ASSERT_EQ(module->elementsec[0].init.size(), 2);
    EXPECT_EQ(module->elementsec[0].init[0], 0);
    EXPECT_EQ(module->elementsec[0].init[1], 1);
    EXPECT_EQ(module->elementsec[1].offset.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module->elementsec[1].offset.value.constant.i32, 2);
    ASSERT_EQ(module->elementsec[1].init.size(), 2);
    EXPECT_EQ(module->elementsec[1].init[0], 2);
    EXPECT_EQ(module->elementsec[1].init[1], 3);
    EXPECT_EQ(module->elementsec[2].offset.kind, ConstantExpression::Kind::GlobalGet);
    EXPECT_EQ(module->elementsec[2].offset.value.global_index, 0);
    ASSERT_EQ(module->elementsec[2].init.size(), 2);
    EXPECT_EQ(module->elementsec[2].init[0], 0);
    EXPECT_EQ(module->elementsec[2].init[1], 3);
}

TEST(parser, element_section_tableidx_nonzero)
{
    const auto section_contents = bytes{0x01, 0x01, 0x41, 0x01, 0x0b, 0x01, 0x00};
    const auto bin = bytes{wasm_prefix} + make_section(9, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "invalid table index 1 (only table 0 is allowed)");
}

TEST(parser, element_section_invalid_initializer)
{
    // tableidx=0 expression=<memory_size> items=0
    const auto wasm = bytes{wasm_prefix} + make_section(9, make_vec({"003f00"_bytes}));
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "unexpected instruction in the constant expression: 63");
}

TEST(parser, element_section_no_table_section)
{
    const auto wasm =
        bytes{wasm_prefix} + make_section(9, make_vec({"0041000b"_bytes + make_vec({"00"_bytes})}));
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error,
        "invalid table index 0 (element section encountered without a table section)");
}

TEST(parser, code_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(10, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->codesec.size(), 0);
}

TEST(parser, code_locals)
{
    const auto wasm_locals = "81017f"_bytes;  // 0x81 x i32.
    const auto wasm =
        bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
        make_section(3, "0100"_bytes) +
        make_section(10, make_vec({add_size_prefix(make_vec({wasm_locals}) + "0b"_bytes)}));

    const auto module = parse(wasm);
    ASSERT_EQ(module->codesec.size(), 1);
    EXPECT_EQ(module->codesec[0].local_count, 0x81);
}

TEST(parser, code_locals_2)
{
    const auto wasm_locals1 = "017e"_bytes;  // 1 x i64.
    const auto wasm_locals2 = "027f"_bytes;  // 2 x i32.
    const auto wasm_locals3 = "037e"_bytes;  // 3 x i64.
    const auto wasm_locals4 = "047e"_bytes;  // 4 x i64.
    const auto wasm =
        bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
        make_section(3, "0100"_bytes) +
        make_section(10,
            make_vec({add_size_prefix(
                make_vec({wasm_locals1, wasm_locals2, wasm_locals3, wasm_locals4}) + "0b"_bytes)}));

    const auto module = parse(wasm);
    ASSERT_EQ(module->codesec.size(), 1);
    EXPECT_EQ(module->codesec[0].local_count, 1 + 2 + 3 + 4);
}

TEST(parser, code_locals_invalid_type)
{
    const auto wasm_locals = "017b"_bytes;  // 1 x <invalid_type>.
    const auto wasm =
        bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
        make_section(3, "0100"_bytes) +
        make_section(10, make_vec({add_size_prefix(make_vec({wasm_locals}) + "0b"_bytes)}));

    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "invalid valtype 123");
}

TEST(parser, code_locals_too_many)
{
    const auto large_num = "8080808008"_bytes;  // 0x80000000
    for (const auto& locals : {
             make_vec({large_num + "7e"_bytes, large_num + "7e"_bytes}),  // large i64 + large i64
             make_vec({large_num + "7e"_bytes, large_num + "7f"_bytes}),  // large i64 + large i32
             make_vec({large_num + "7f"_bytes, large_num + "7f"_bytes})   // large i32 + large i32
         })
    {
        const auto wasm = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
                          make_section(3, "0100"_bytes) +
                          make_section(10, make_vec({add_size_prefix(locals + "0b"_bytes)}));

        EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "too many local variables");
    }
}

TEST(parser, code_with_empty_expr_2_locals)
{
    // Func with 2x i32 locals, only 0x0b "end" instruction.
    const auto func_2_locals_bin = "01027f0b"_bytes;
    const auto code_bin = add_size_prefix(func_2_locals_bin);
    const auto wasm_bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
                          make_section(3, "0100"_bytes) + make_section(10, make_vec({code_bin}));

    const auto module = parse(wasm_bin);
    ASSERT_EQ(module->codesec.size(), 1);
    const auto& code_obj = module->codesec[0];
    EXPECT_EQ(code_obj.local_count, 2);
    EXPECT_THAT(code_obj.instructions, ElementsAre(Instr::end));
}

TEST(parser, code_with_empty_expr_5_locals)
{
    // Func with 1x i64 + 4x i32 locals , only 0x0b "end" instruction.
    const auto func_5_locals_bin = "02017f047e0b"_bytes;
    const auto code_bin = add_size_prefix(func_5_locals_bin);
    const auto wasm_bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
                          make_section(3, "0100"_bytes) + make_section(10, make_vec({code_bin}));

    const auto module = parse(wasm_bin);
    ASSERT_EQ(module->codesec.size(), 1);
    const auto& code_obj = module->codesec[0];
    EXPECT_EQ(code_obj.local_count, 5);
    EXPECT_THAT(code_obj.instructions, ElementsAre(Instr::end));
}

TEST(parser, code_section_with_2_trivial_codes)
{
    const auto func_nolocals_bin = "000b"_bytes;
    const auto code_bin = add_size_prefix(func_nolocals_bin);
    const auto section_contents = make_vec({code_bin, code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
                     make_section(3, "020000"_bytes) + make_section(10, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module->typesec.size(), 1);
    EXPECT_EQ(module->typesec[0].inputs.size(), 0);
    EXPECT_EQ(module->typesec[0].outputs.size(), 0);
    ASSERT_EQ(module->codesec.size(), 2);
    EXPECT_EQ(module->codesec[0].local_count, 0);
    EXPECT_THAT(module->codesec[0].instructions, ElementsAre(Instr::end));
    EXPECT_EQ(module->codesec[1].local_count, 0);
    EXPECT_THAT(module->codesec[1].instructions, ElementsAre(Instr::end));
}

TEST(parser, code_section_with_basic_instructions)
{
    const auto func_bin =
        "01047f"  // vec(locals)
        "2001"
        "4102"
        "6a"
        "2103"
        "01"
        "00"
        "0b"_bytes;
    const auto code_bin = add_size_prefix(func_bin);
    const auto section_contents = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
                     make_section(3, "0100"_bytes) + make_section(10, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module->typesec.size(), 1);
    EXPECT_EQ(module->typesec[0].inputs.size(), 0);
    EXPECT_EQ(module->typesec[0].outputs.size(), 0);
    ASSERT_EQ(module->codesec.size(), 1);
    EXPECT_EQ(module->codesec[0].local_count, 4);
    EXPECT_THAT(module->codesec[0].instructions,
        ElementsAre(Instr::local_get, 1, 0, 0, 0, Instr::i32_const, 2, 0, 0, 0, Instr::i32_add,
            Instr::local_set, 3, 0, 0, 0, Instr::nop, Instr::unreachable, Instr::end));
}

TEST(parser, code_section_with_memory_size)
{
    const auto func_bin =
        "00"  // vec(locals)
        "3f000b"_bytes;
    const auto code_bin = add_size_prefix(func_bin);
    const auto section_contents = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {0x7f})})) +
                     make_section(3, "0100"_bytes) + make_section(5, make_vec({"0000"_bytes})) +
                     make_section(10, section_contents);
    const auto module = parse(bin);
    ASSERT_EQ(module->codesec.size(), 1);
    EXPECT_EQ(module->codesec[0].local_count, 0);
    EXPECT_THAT(module->codesec[0].instructions, ElementsAre(Instr::memory_size, Instr::end));

    const auto func_bin_invalid =
        "00"  // vec(locals)
        "3f010b"_bytes;
    const auto code_bin_invalid = add_size_prefix(func_bin_invalid);
    const auto section_contents_invalid = make_vec({code_bin_invalid});
    const auto bin_invalid =
        bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
        make_section(3, "0100"_bytes) + make_section(10, section_contents_invalid);

    EXPECT_THROW_MESSAGE(parse(bin_invalid), parser_error, "invalid memory index encountered");
}

TEST(parser, code_section_with_memory_grow)
{
    const auto func_bin = "00"_bytes +  // vec(locals)
                          i32_const(0) + "40001a0b"_bytes;
    const auto code_bin = add_size_prefix(func_bin);
    const auto code_section = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
                     make_section(3, "0100"_bytes) + make_section(5, make_vec({"0000"_bytes})) +
                     make_section(10, code_section);

    const auto module = parse(bin);
    ASSERT_EQ(module->codesec.size(), 1);
    EXPECT_EQ(module->codesec[0].local_count, 0);
    EXPECT_THAT(module->codesec[0].instructions,
        ElementsAre(Instr::i32_const, 0, 0, 0, 0, Instr::memory_grow, Instr::drop, Instr::end));

    const auto func_bin_invalid = "00"_bytes +  // vec(locals)
                                  i32_const(0) + "40011a0b"_bytes;
    const auto code_bin_invalid = add_size_prefix(func_bin_invalid);
    const auto code_section_invalid = make_vec({code_bin_invalid});
    const auto bin_invalid = bytes{wasm_prefix} +
                             make_section(1, make_vec({make_functype({}, {})})) +
                             make_section(3, "0100"_bytes) + make_section(10, code_section_invalid);

    EXPECT_THROW_MESSAGE(parse(bin_invalid), parser_error, "invalid memory index encountered");
}

TEST(parser, code_section_fp_instructions)
{
    const uint8_t fp_instructions[] = {0x2a, 0x2b, 0x38, 0x39, 0x43, 0x44, 0x5b, 0x5c, 0x5d, 0x5e,
        0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91,
        0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
        0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa8, 0xa9, 0xaa, 0xab, 0xae, 0xaf, 0xb0, 0xb1, 0xb2,
        0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf};

    const auto types_table = fizzy::get_instruction_type_table();

    for (const auto instr : fp_instructions)
    {
        const auto types = types_table[instr];

        auto func_bin = "00"_bytes;  // vec(locals)
        for (const auto input : types.inputs)
        {
            switch (input)
            {
            case (ValType::i32):
                func_bin += i32_const(0);
                break;
            case (ValType::i64):
                func_bin += i64_const(0);
                break;
            case (ValType::f32):
                func_bin += uint8_t(Instr::f32_const) + bytes(4, 0);
                break;
            case (ValType::f64):
                func_bin += uint8_t(Instr::f64_const) + bytes(8, 0);
                break;
            }
        }
        func_bin += bytes{instr};

        switch (instr)
        {
        case 0x2a:  // f32.load
        case 0x2b:  // f64.load
        case 0x38:  // f32.store
        case 0x39:  // f64.store
            func_bin += test::leb128u_encode(0) + test::leb128u_encode(0);
            break;
        case 0x43:  // f32.const
            func_bin += bytes(4, 0);
            break;
        case 0x44:  // f64.const
            func_bin += bytes(8, 0);
            break;
        }

        if (types.outputs.size() == 1)
            func_bin += bytes{0x1a};  // drop
        func_bin += bytes{0xb};       // end

        const auto code_bin = add_size_prefix(func_bin);
        const auto code_section = make_vec({code_bin});
        const auto function_section = make_vec({"00"_bytes});
        const auto bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
                         make_section(3, function_section) +
                         make_section(5, make_vec({"0000"_bytes})) + make_section(10, code_section);

        const auto module = parse(bin);
        EXPECT_EQ(module->codesec.size(), 1);
    }
}

TEST(parser, code_section_invalid_instructions)
{
    const uint8_t invalid_instructions[] = {0x06, 0x07, 0x08, 0x09, 0x0a, 0x12, 0x13, 0x14, 0x15,
        0x16, 0x17, 0x18, 0x19, 0x25, 0x26, 0x27, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7,
        0xc8, 0xc9, 0xca, 0xcb, 0xcc, 0xcd, 0xce, 0xcf, 0xd0, 0xd1, 0xd2, 0xd3, 0xd4, 0xd5, 0xd6,
        0xd7, 0xd8, 0xd9, 0xda, 0xdb, 0xdc, 0xdd, 0xde, 0xdf, 0xe0, 0xe1, 0xe2, 0xe3, 0xe4, 0xe5,
        0xe6, 0xe7, 0xe8, 0xe9, 0xea, 0xeb, 0xec, 0xed, 0xee, 0xef, 0xf0, 0xf1, 0xf2, 0xf3, 0xf4,
        0xf5, 0xf6, 0xf7, 0xf8, 0xf9, 0xfa, 0xfb, 0xfc, 0xfd, 0xfe, 0xff};

    for (const auto instr : invalid_instructions)
    {
        const auto func_bin = "00"_bytes  // vec(locals)
                              + bytes{instr};
        const auto code_bin = add_size_prefix(func_bin);
        const auto section_contents = make_vec({code_bin});
        const auto bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
                         make_section(3, make_vec({"00"_bytes})) +
                         make_section(10, section_contents);

        const auto expected_msg = std::string{"invalid instruction "} + std::to_string(instr);
        EXPECT_THROW_MESSAGE(parse(bin), parser_error, expected_msg.c_str());
    }
}

TEST(parser, code_section_size_too_small)
{
    // Real size is 5 bytes
    const auto func_bin =
        "00"  // vec(locals)
        "0101010b"_bytes;
    const auto code_bin = "04"_bytes + func_bin;
    const auto section_contents = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(10, section_contents);

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "incorrect section 10 size, difference: -1");
}

TEST(parser, code_section_size_too_large)
{
    // Real size is 5 bytes
    const auto func_bin =
        "00"  // vec(locals)
        "0101010b"_bytes;
    const auto code_bin = "06"_bytes + func_bin;
    const auto section_contents = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(10, section_contents);

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "unexpected EOF");
}

TEST(parser, code_section_with_unused_bytes)
{
    // Code size is correctly 6, but expr only consumes 5 bytes (additional zero byte of extension).
    const auto func_bin =
        "00"  // vec(locals)
        "0101010b"_bytes;
    const auto code_bin = "06"_bytes + func_bin + "00"_bytes;
    const auto section_contents = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(1, make_vec({make_functype({}, {})})) +
                     make_section(3, make_vec({"00"_bytes})) + make_section(10, section_contents);

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "malformed size field for function");
}

TEST(parser, code_section_without_function_section)
{
    const auto code_bin = "02000b"_bytes;
    const auto section_contents = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(10, section_contents);

    EXPECT_THROW_MESSAGE(parse(bin), parser_error,
        "malformed binary: number of function and code entries must match");
}

TEST(parser, data_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(5, make_vec({"0000"_bytes})) +
                     make_section(11, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module->datasec.size(), 0);
}

TEST(parser, data_section)
{
    /* wat2wasm
      (global (import "m" "g") i32)
      (memory 0)
      (data (i32.const 1) "\aa\ff")
      (data (i32.const 2) "\55\55")
      (data (global.get 0) "\24\24")
    */
    const auto bin = from_hex(
        "0061736d01000000020801016d0167037f0005030100000b16030041010b02aaff0041020b0255550023000b02"
        "2424");
    const auto module = parse(bin);

    ASSERT_EQ(module->datasec.size(), 3);
    EXPECT_EQ(module->datasec[0].offset.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module->datasec[0].offset.value.constant.i32, 1);
    EXPECT_EQ(module->datasec[0].init, "aaff"_bytes);
    EXPECT_EQ(module->datasec[1].offset.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module->datasec[1].offset.value.constant.i32, 2);
    EXPECT_EQ(module->datasec[1].init, "5555"_bytes);
    EXPECT_EQ(module->datasec[2].offset.kind, ConstantExpression::Kind::GlobalGet);
    EXPECT_EQ(module->datasec[2].offset.value.global_index, 0);
    EXPECT_EQ(module->datasec[2].init, "2424"_bytes);
}

TEST(parser, data_section_memidx_nonzero)
{
    const auto section_contents = make_vec({"0141010b0100"_bytes});
    const auto bin = bytes{wasm_prefix} + make_section(11, section_contents);
    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "invalid memory index 1 (only memory 0 is allowed)");
}

TEST(parser, data_section_invalid_initializer)
{
    // memidx=0 expression=<memory_size> items=0
    const auto wasm = bytes{wasm_prefix} + make_section(9, make_vec({"003f00"_bytes}));
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "unexpected instruction in the constant expression: 63");
}

TEST(parser, data_section_empty_vector_without_memory)
{
    const auto bin = bytes{wasm_prefix} + make_section(11, make_vec({"0041010b00"_bytes}));
    EXPECT_THROW_MESSAGE(parse(bin), validation_error,
        "invalid memory index 0 (data section encountered without a memory section)");
}

TEST(parser, data_section_without_memory)
{
    const auto bin = bytes{wasm_prefix} + make_section(11, make_vec({"0041010b02aaff"_bytes}));
    EXPECT_THROW_MESSAGE(parse(bin), validation_error,
        "invalid memory index 0 (data section encountered without a memory section)");
}

TEST(parser, data_section_out_of_bounds)
{
    // Expected data of length 1, 0 bytes given.
    const auto wasm1 = bytes{wasm_prefix} + make_section(11, make_vec({"0041000b01"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm1), parser_error, "unexpected EOF");

    // Expected data of length 9, 8 bytes given.
    const auto wasm2 =
        bytes{wasm_prefix} + make_section(11, make_vec({"0041000b09d0d1d2d3d4d5d6d7"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm2), parser_error, "unexpected EOF");

    // Expected data of length 127 (0x7f), 1 byte given. This detects invalid pointer comparison
    // with ASan pointer-compare,pointer-subtract.
    const auto wasm3 = bytes{wasm_prefix} + make_section(11, make_vec({"0041000b7fd0"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm3), parser_error, "unexpected EOF");
}

TEST(parser, unknown_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(12, bytes{});
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "unknown section encountered 12");
}

TEST(parser, unknown_section_nonempty)
{
    const auto bin =
        bytes{wasm_prefix} + make_section(13, "ff"_bytes) + make_section(12, "ff42ff"_bytes);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "unknown section encountered 13");
}

TEST(parser, interleaved_custom_section)
{
    const auto type_section = make_vec({make_functype({}, {})});
    const auto func_section = make_vec({"00"_bytes});
    const auto code_section = make_vec({add_size_prefix("000b"_bytes)});
    const auto bin = bytes{wasm_prefix} + make_section(0, "0161"_bytes) +
                     make_section(1, type_section) + make_section(0, "0162"_bytes) +
                     make_section(3, func_section) + make_section(0, "0163"_bytes) +
                     make_section(10, code_section);

    const auto module = parse(bin);
    EXPECT_EQ(module->typesec.size(), 1);
    EXPECT_EQ(module->funcsec.size(), 1);
    EXPECT_EQ(module->codesec.size(), 1);
}

TEST(parser, wrongly_ordered_sections)
{
    auto create_empty_section = [](uint8_t id) -> bytes {
        if (id == 8)  // start
            return make_section(8, "00"_bytes);
        else
            return make_section(id, make_vec({}));
    };

    constexpr uint8_t first_section_id = 1;
    constexpr uint8_t last_section_id = 11;

    for (auto id = last_section_id; id > first_section_id; id--)
    {
        const auto wrong_order_bin =
            bytes{wasm_prefix} + create_empty_section(0) + create_empty_section(id) +
            create_empty_section(0) + create_empty_section(0) +
            create_empty_section(uint8_t(id - 1)) + create_empty_section(0);
        EXPECT_THROW_MESSAGE(
            parse(wrong_order_bin), parser_error, "unexpected out-of-order section type");

        const auto duplicated_bin = bytes{wasm_prefix} + create_empty_section(0) +
                                    create_empty_section(id) + create_empty_section(0) +
                                    create_empty_section(id) + create_empty_section(0);
        EXPECT_THROW_MESSAGE(
            parse(duplicated_bin), parser_error, "unexpected out-of-order section type");
    }
}

TEST(parser, milestone1)
{
    /* wat2wasm
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
    const auto wasm = from_hex(
        "0061736d0100000001070160027f7f017f030201000a13011101017f200020016a20026a220220006a0b");
    const auto m = parse(wasm);

    ASSERT_EQ(m->typesec.size(), 1);
    EXPECT_THAT(m->typesec[0].inputs, ElementsAre(ValType::i32, ValType::i32));
    EXPECT_THAT(m->typesec[0].outputs, ElementsAre(ValType::i32));

    ASSERT_EQ(m->codesec.size(), 1);
    const auto& c = m->codesec[0];
    EXPECT_EQ(c.local_count, 1);
    EXPECT_THAT(c.instructions,
        ElementsAre(Instr::local_get, 0, 0, 0, 0, Instr::local_get, 1, 0, 0, 0, Instr::i32_add,
            Instr::local_get, 2, 0, 0, 0, Instr::i32_add, Instr::local_tee, 2, 0, 0, 0,
            Instr::local_get, 0, 0, 0, 0, Instr::i32_add, Instr::end));
}
