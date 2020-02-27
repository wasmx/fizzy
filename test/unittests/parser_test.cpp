#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>
#include <array>

using namespace fizzy;

static const auto functype_void_to_void = "600000"_bytes;
static const auto functype_i32i64_to_i32 = "60027f7e017f"_bytes;
static const auto functype_i32_to_void = "60017f00"_bytes;

namespace
{
bytes add_size_prefix(const bytes& content)
{
    assert(content.size() < 0x80);
    return bytes{static_cast<uint8_t>(content.size())} + content;
}

bytes make_vec(std::initializer_list<bytes_view> contents)
{
    assert(contents.size() < 0x80);
    bytes ret{static_cast<uint8_t>(contents.size())};
    for (const auto& content : contents)
        ret.append(content);
    return ret;
}

bytes make_section(uint8_t id, const bytes& content)
{
    return bytes{id} + add_size_prefix(content);
}

bytes make_invalid_size_section(uint8_t id, size_t size, const bytes& content)
{
    assert(size < 0x80);
    return bytes{id, static_cast<uint8_t>(size)} + content;
}
}  // namespace

TEST(parser, valtype)
{
    std::array<uint8_t, 1> b{};
    b[0] = 0x7e;
    EXPECT_EQ(std::get<0>(parse<ValType>(b.begin(), b.end())), ValType::i64);
    b[0] = 0x7f;
    EXPECT_EQ(std::get<0>(parse<ValType>(b.begin(), b.end())), ValType::i32);
    b[0] = 0x7c;
    EXPECT_THROW_MESSAGE(
        parse<ValType>(b.begin(), b.end()), parser_error, "unsupported valtype (floating point)");
    b[0] = 0x7d;
    EXPECT_THROW_MESSAGE(
        parse<ValType>(b.begin(), b.end()), parser_error, "unsupported valtype (floating point)");
    b[0] = 0x7a;
    EXPECT_THROW_MESSAGE(parse<ValType>(b.begin(), b.end()), parser_error, "invalid valtype 122");
}

TEST(parser, valtype_vec)
{
    const auto input = "037f7e7fcc"_bytes;
    const auto [vec, pos] = parse_vec<ValType>(input.data(), input.data() + input.size());
    EXPECT_EQ(pos, input.data() + 4);
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], ValType::i32);
    EXPECT_EQ(vec[1], ValType::i64);
    EXPECT_EQ(vec[2], ValType::i32);
}

TEST(parser, limits_min)
{
    const auto input = "007f"_bytes;
    const auto [limits, pos] = parse_limits(input.data(), input.data() + input.size());
    EXPECT_EQ(limits.min, 0x7f);
    EXPECT_FALSE(limits.max.has_value());
}

TEST(parser, limits_minmax)
{
    const auto input = "01207f"_bytes;
    const auto [limits, pos] = parse_limits(input.data(), input.data() + input.size());
    EXPECT_EQ(limits.min, 0x20);
    EXPECT_TRUE(limits.max.has_value());
    EXPECT_EQ(*limits.max, 0x7f);
}

TEST(parser, limits_min_invalid_too_short)
{
    const auto input = "00"_bytes;
    EXPECT_THROW_MESSAGE(
        parse_limits(input.data(), input.data() + input.size()), parser_error, "Unexpected EOF");
}

TEST(parser, limits_minmax_invalid_too_short)
{
    const auto input = "0120"_bytes;
    EXPECT_THROW_MESSAGE(
        parse_limits(input.data(), input.data() + input.size()), parser_error, "Unexpected EOF");
}

TEST(parser, limits_invalid)
{
    const auto input = "02"_bytes;
    EXPECT_THROW_MESSAGE(
        parse_limits(input.data(), input.data() + input.size()), parser_error, "invalid limits 2");
}

TEST(parser, module_empty)
{
    const auto module = parse(wasm_prefix);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, module_with_wrong_prefix)
{
    EXPECT_THROW_MESSAGE(parse({}), parser_error, "invalid wasm module prefix");
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
        EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
    }
}

TEST(parser, custom_section_empty)
{
    // Section consists of an empty name
    const auto bin = bytes{wasm_prefix} + make_section(0, "00"_bytes);
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, custom_section_nonempty_name_only)
{
    // Section consists of only the name "abc"
    const auto bin = bytes{wasm_prefix} + make_section(0, "03616263"_bytes);
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, custom_section_nonempty)
{
    // Section consists of the name "abc" and 14 bytes of unparsed data
    const auto bin =
        bytes{wasm_prefix} + make_section(0, "036162630000112233445566778899000099"_bytes);
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, custom_section_size_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + "0080"_bytes;
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, custom_section_name_out_of_bounds)
{
    const auto bin = bytes{wasm_prefix} + make_section(0, "01"_bytes);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "Unexpected EOF");
}

TEST(parser, custom_section_name_exceeds_section_size)
{
    const auto bin = bytes{wasm_prefix} + make_invalid_size_section(0, 1, "01aa"_bytes) +
                     make_section(0, "00"_bytes);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "Unexpected EOF");
}

TEST(parser, custom_section_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_invalid_size_section(0, 31, {});
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, custom_section_invalid_utf8)
{
    const auto bin = bytes{wasm_prefix} + make_section(0, "027f80"_bytes);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "Invalid UTF-8");
}

TEST(parser, type_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(1, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
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
    const auto section_contents = "01"_bytes + functype_void_to_void;
    const auto bin =
        bytes{wasm_prefix} +
        make_invalid_size_section(1, size_t{section_contents.size() - 1}, section_contents);
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "incorrect section 1 size, difference: 1");
}

TEST(parser, type_section_smaller_than_expected)
{
    const auto section_contents = "01"_bytes + functype_void_to_void + "fe"_bytes;
    const auto bin =
        bytes{wasm_prefix} +
        make_invalid_size_section(1, size_t{section_contents.size() + 1}, section_contents) +
        "00"_bytes;
    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "incorrect section 1 size, difference: -2");
}

TEST(parser, type_section_with_single_functype)
{
    // single type [void] -> [void]
    const auto section_contents = "01"_bytes + functype_void_to_void;
    const auto bin = bytes{wasm_prefix} + make_section(1, section_contents);
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
    const auto section_contents = make_vec({functype_i32i64_to_i32});
    const auto bin = bytes{wasm_prefix} + make_section(1, section_contents);
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
        "03"_bytes + functype_void_to_void + functype_i32i64_to_i32 + functype_i32_to_void;
    const auto bin = bytes{wasm_prefix} + make_section(1, section_contents);

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

TEST(parser, type_section_functype_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(1, make_vec({""_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, import_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(2, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.importsec.size(), 0);
}

TEST(parser, import_single_function)
{
    const auto section_contents = bytes{0x01, 0x03, 'm', 'o', 'd', 0x03, 'f', 'o', 'o', 0x00, 0x42};
    const auto bin = bytes{wasm_prefix} + make_section(2, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.importsec.size(), 1);
    EXPECT_EQ(module.importsec[0].module, "mod");
    EXPECT_EQ(module.importsec[0].name, "foo");
    EXPECT_EQ(module.importsec[0].kind, ExternalKind::Function);
    EXPECT_EQ(module.importsec[0].desc.function_type_index, 0x42);
}

TEST(parser, import_multiple)
{
    const auto section_contents = make_vec({bytes{0x02, 'm', '1', 0x03, 'a', 'b', 'c', 0x00, 0x42},
        bytes{0x02, 'm', '2', 0x03, 'f', 'o', 'o', 0x02, 0x00, 0x7f},
        bytes{0x02, 'm', '3', 0x03, 'b', 'a', 'r', 0x03, 0x7f, 0x00},
        bytes{0x02, 'm', '4', 0x03, 't', 'a', 'b', 0x01, 0x70, 0x01, 0x01, 0x42}});
    const auto bin = bytes{wasm_prefix} + make_section(2, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.importsec.size(), 4);
    EXPECT_EQ(module.importsec[0].module, "m1");
    EXPECT_EQ(module.importsec[0].name, "abc");
    EXPECT_EQ(module.importsec[0].kind, ExternalKind::Function);
    EXPECT_EQ(module.importsec[0].desc.function_type_index, 0x42);
    EXPECT_EQ(module.importsec[1].module, "m2");
    EXPECT_EQ(module.importsec[1].name, "foo");
    EXPECT_EQ(module.importsec[1].kind, ExternalKind::Memory);
    EXPECT_EQ(module.importsec[1].desc.memory.limits.min, 0x7f);
    EXPECT_FALSE(module.importsec[1].desc.memory.limits.max);
    EXPECT_EQ(module.importsec[2].module, "m3");
    EXPECT_EQ(module.importsec[2].name, "bar");
    EXPECT_EQ(module.importsec[2].kind, ExternalKind::Global);
    EXPECT_FALSE(module.importsec[2].desc.global_mutable);
    EXPECT_EQ(module.importsec[3].module, "m4");
    EXPECT_EQ(module.importsec[3].name, "tab");
    EXPECT_EQ(module.importsec[3].kind, ExternalKind::Table);
    EXPECT_EQ(module.importsec[3].desc.table.limits.min, 1);
    ASSERT_TRUE(module.importsec[3].desc.table.limits.max.has_value());
    EXPECT_EQ(*module.importsec[3].desc.table.limits.max, 0x42);
}

TEST(parser, import_memories_multiple)
{
    const auto section_contents =
        make_vec({bytes{0x02, 'm', '1', 0x03, 'a', 'b', 'c', 0x02, 0x00, 0x7f},
            bytes{0x02, 'm', '2', 0x03, 'd', 'e', 'f', 0x02, 0x00, 0x7f}});
    const auto bin = bytes{wasm_prefix} + make_section(2, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), parser_error, "too many imported memories (at most one is allowed)");
}

TEST(parser, import_invalid_kind)
{
    const auto wasm = bytes{wasm_prefix} + make_section(2, make_vec({"000004"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected import kind value 4");
}

TEST(parser, import_kind_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(2, make_vec({"0000"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, import_invalid_utf8_in_module)
{
    const auto section_contents =
        bytes{0x01, 0x03, 'm', 0x80, 'd', 0x03, 'f', 'o', 'o', 0x00, 0x42};
    const auto wasm = bytes{wasm_prefix} + make_section(2, section_contents);
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Invalid UTF-8");
}

TEST(parser, import_invalid_utf8_in_name)
{
    const auto section_contents =
        bytes{0x01, 0x03, 'm', 'o', 'd', 0x03, 'f', 0x80, 'o', 0x00, 0x42};
    const auto wasm = bytes{wasm_prefix} + make_section(2, section_contents);
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Invalid UTF-8");
}

TEST(parser, memory_and_imported_memory)
{
    // (import "js" "mem"(memory 1))
    const auto import_section = "020b01026a73036d656d0200010008046e616d65020100"_bytes;
    // (memory 1)
    const auto memory_section = "05030100010008046e616d65020100"_bytes;
    const auto bin = bytes{wasm_prefix} + import_section + memory_section;

    EXPECT_THROW_MESSAGE(parse(bin), parser_error,
        "both module memory and imported memory are defined (at most one of them is allowed)");
}

TEST(parser, import_tables_multiple)
{
    const auto section_contents =
        make_vec({bytes{0x02, 'm', '1', 0x03, 'a', 'b', 'c', 0x01, 0x70, 0x00, 0x01},
            bytes{0x02, 'm', '2', 0x03, 'd', 'e', 'f', 0x01, 0x70, 0x01, 0x01, 0x03}});
    const auto bin = bytes{wasm_prefix} + make_section(2, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), parser_error, "too many imported tables (at most one is allowed)");
}

TEST(parser, table_and_imported_table)
{
    // (import "js" "t" (table 1 anyfunc))
    const auto import_section = "020a01026a730174017000010008046e616d65020100"_bytes;
    // (table 2 anyfunc)
    const auto table_section = "0404017000020008046e616d65020100"_bytes;
    const auto bin = bytes{wasm_prefix} + import_section + table_section;

    EXPECT_THROW_MESSAGE(parse(bin), parser_error,
        "both module table and imported table are defined (at most one of them is allowed)");
}

TEST(parser, function_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(3, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.funcsec.size(), 0);
}

TEST(parser, function_section_with_single_function)
{
    const auto section_contents = "0100"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(3, section_contents);
    const auto module = parse(bin);
    ASSERT_EQ(module.funcsec.size(), 1);
    EXPECT_EQ(module.funcsec[0], 0);
}

TEST(parser, function_section_with_multiple_functions)
{
    const auto section_contents = "04000142ff01"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(3, section_contents);
    const auto module = parse(bin);
    ASSERT_EQ(module.funcsec.size(), 4);
    EXPECT_EQ(module.funcsec[0], 0);
    EXPECT_EQ(module.funcsec[1], 1);
    EXPECT_EQ(module.funcsec[2], 0x42);
    EXPECT_EQ(module.funcsec[3], 0xff);
}

TEST(parser, function_section_end_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_invalid_size_section(3, 2, {});
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, table_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(4, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.tablesec.size(), 0);
}

TEST(parser, table_single_min_limit)
{
    const auto section_contents = "0170007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.tablesec.size(), 1);
    EXPECT_EQ(module.tablesec[0].limits.min, 0x7f);
}

TEST(parser, table_single_minmax_limit)
{
    const auto section_contents = "017001127f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.tablesec.size(), 1);
    EXPECT_EQ(module.tablesec[0].limits.min, 0x12);
    EXPECT_EQ(module.tablesec[0].limits.max, 0x7f);
}

// Where minimum exceeds maximum
TEST(parser, table_single_malformed_minmax)
{
    const auto section_contents = "0170017f12"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), parser_error, "malformed limits (minimum is larger than maximum)");
}

TEST(parser, table_multi_min_limit)
{
    const auto section_contents = "0270007f70007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), parser_error, "too many table sections (at most one is allowed)");
}

TEST(parser, table_invalid_elemtype)
{
    const auto wasm = bytes{wasm_prefix} + make_section(4, make_vec({"71"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected table elemtype: 113");
}

TEST(parser, table_elemtype_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(4, make_vec({""_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, memory_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(5, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.memorysec.size(), 0);
}

TEST(parser, memory_single_min_limit)
{
    const auto section_contents = "01007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.memorysec.size(), 1);
    EXPECT_EQ(module.memorysec[0].limits.min, 0x7f);
}

TEST(parser, memory_single_minmax_limit)
{
    const auto section_contents = "0101127f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.memorysec.size(), 1);
    EXPECT_EQ(module.memorysec[0].limits.min, 0x12);
    EXPECT_EQ(module.memorysec[0].limits.max, 0x7f);
}

// Where minimum exceeds maximum
TEST(parser, memory_single_malformed_minmax)
{
    const auto section_contents = "01017f12"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), parser_error, "malformed limits (minimum is larger than maximum)");
}

TEST(parser, memory_multi_min_limit)
{
    const auto section_contents = "02007f007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), parser_error, "too many memory sections (at most one is allowed)");
}

TEST(parser, memory_limits_kind_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(5, make_vec({""_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, global_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(6, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.globalsec.size(), 0);
}

TEST(parser, global_single_mutable_const_inited)
{
    const auto section_contents = bytes{0x01, 0x7f, 0x01, uint8_t(Instr::i32_const), 0x10, 0x0b};
    const auto bin = bytes{wasm_prefix} + make_section(6, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.globalsec.size(), 1);
    EXPECT_TRUE(module.globalsec[0].is_mutable);
    EXPECT_EQ(module.globalsec[0].expression.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module.globalsec[0].expression.value.constant, 0x10);
}

TEST(parser, global_single_const_global_inited)
{
    const auto section_contents = bytes{0x01, 0x7f, 0x00, uint8_t(Instr::global_get), 0x01, 0x0b};
    const auto bin = bytes{wasm_prefix} + make_section(6, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.globalsec.size(), 1);
    EXPECT_FALSE(module.globalsec[0].is_mutable);
    EXPECT_EQ(module.globalsec[0].expression.kind, ConstantExpression::Kind::GlobalGet);
    EXPECT_EQ(module.globalsec[0].expression.value.global_index, 0x01);
}

TEST(parser, global_single_multi_instructions_inited)
{
    const auto section_contents = bytes{
        0x01, 0x7f, 0x01, uint8_t(Instr::i32_const), 0x10, uint8_t(Instr::i64_const), 0x7f, 0x0b};
    const auto bin = bytes{wasm_prefix} + make_section(6, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.globalsec.size(), 1);
    EXPECT_TRUE(module.globalsec[0].is_mutable);
    EXPECT_EQ(module.globalsec[0].expression.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module.globalsec[0].expression.value.constant, uint64_t(-1));
}

TEST(parser, global_multi_const_inited)
{
    const auto section_contents =
        make_vec({bytes{0x7f, 0x00, uint8_t(Instr::i32_const), 0x01, 0x0b},
            bytes{0x7f, 0x01, uint8_t(Instr::i32_const), 0x7f, 0x0b}});
    const auto bin = bytes{wasm_prefix} + make_section(6, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.globalsec.size(), 2);
    EXPECT_FALSE(module.globalsec[0].is_mutable);
    EXPECT_EQ(module.globalsec[0].expression.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module.globalsec[0].expression.value.constant, 0x01);
    EXPECT_TRUE(module.globalsec[1].is_mutable);
    EXPECT_EQ(module.globalsec[1].expression.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module.globalsec[1].expression.value.constant, uint32_t(-1));
}

TEST(parser, global_invalid_mutability)
{
    const auto wasm = bytes{wasm_prefix} + make_section(6, make_vec({"7f02"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error,
        "unexpected byte value 2, expected 0x00 or 0x01 for global mutability");
}

TEST(parser, global_initializer_expression_invalid_instruction)
{
    const auto wasm = bytes{wasm_prefix} + make_section(6, make_vec({"7f0000"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error,
        "unexpected instruction in the global initializer expression: 0");
}

TEST(parser, global_valtype_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(6, make_vec({""_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, global_mutability_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(6, make_vec({"7f"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, global_constant_expression_out_of_bounds)
{
    // i32, immutable, EOF.
    const auto wasm1 = bytes{wasm_prefix} + make_section(6, make_vec({"7f00"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm1), parser_error, "Unexpected EOF");

    // i32, immutable, i32_const, 0, EOF.
    const auto wasm2 = bytes{wasm_prefix} + make_section(6, make_vec({"7f004100"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm2), parser_error, "Unexpected EOF");

    // i32, immutable, i32_const, 0x81, EOF.
    const auto wasm3 = bytes{wasm_prefix} + make_section(6, make_vec({"7f004181"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm3), parser_error, "Unexpected EOF");

    // i32, immutable, i64_const, 0x808081, EOF.
    const auto wasm4 = bytes{wasm_prefix} + make_section(6, make_vec({"7f0042808081"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm4), parser_error, "Unexpected EOF");
}

TEST(parser, export_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(7, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.exportsec.size(), 0);
}

TEST(parser, export_single_function)
{
    const auto section_contents = make_vec({bytes{0x03, 'a', 'b', 'c', 0x00, 0x42}});
    const auto bin = bytes{wasm_prefix} + make_section(7, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.exportsec.size(), 1);
    EXPECT_EQ(module.exportsec[0].name, "abc");
    EXPECT_EQ(module.exportsec[0].kind, ExternalKind::Function);
    EXPECT_EQ(module.exportsec[0].index, 0x42);
}

TEST(parser, export_multiple)
{
    const auto section_contents =
        make_vec({bytes{0x03, 'a', 'b', 'c', 0x00, 0x42}, bytes{0x03, 'f', 'o', 'o', 0x01, 0x43},
            bytes{0x03, 'b', 'a', 'r', 0x02, 0x44}, bytes{0x03, 'x', 'y', 'z', 0x03, 0x45}});
    const auto bin = bytes{wasm_prefix} + make_section(7, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.exportsec.size(), 4);
    EXPECT_EQ(module.exportsec[0].name, "abc");
    EXPECT_EQ(module.exportsec[0].kind, ExternalKind::Function);
    EXPECT_EQ(module.exportsec[0].index, 0x42);
    EXPECT_EQ(module.exportsec[1].name, "foo");
    EXPECT_EQ(module.exportsec[1].kind, ExternalKind::Table);
    EXPECT_EQ(module.exportsec[1].index, 0x43);
    EXPECT_EQ(module.exportsec[2].name, "bar");
    EXPECT_EQ(module.exportsec[2].kind, ExternalKind::Memory);
    EXPECT_EQ(module.exportsec[2].index, 0x44);
    EXPECT_EQ(module.exportsec[3].name, "xyz");
    EXPECT_EQ(module.exportsec[3].kind, ExternalKind::Global);
    EXPECT_EQ(module.exportsec[3].index, 0x45);
}

TEST(parser, export_invalid_kind)
{
    const auto wasm = bytes{wasm_prefix} + make_section(7, make_vec({"0004"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "unexpected export kind value 4");
}

TEST(parser, export_kind_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(7, make_vec({"00"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, export_invalid_utf8)
{
    const auto section_contents = make_vec({bytes{0x03, 'a', 0x80, 'c', 0x00, 0x42}});
    const auto wasm = bytes{wasm_prefix} + make_section(7, section_contents);
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Invalid UTF-8");
}

TEST(parser, export_name_out_of_bounds)
{
    const auto wasm1 = bytes{wasm_prefix} + make_section(7, make_vec({"01"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm1), parser_error, "Unexpected EOF");

    const auto wasm2 = bytes{wasm_prefix} + make_section(7, make_vec({"7faabbccddeeff"_bytes}));
    EXPECT_THROW_MESSAGE(parse(wasm2), parser_error, "Unexpected EOF");
}

TEST(parser, start)
{
    const auto func_section = make_vec({"00"_bytes, "00"_bytes});
    const auto start_section = "01"_bytes;
    const auto bin =
        bytes{wasm_prefix} + make_section(3, func_section) + make_section(8, start_section);

    const auto module = parse(bin);
    EXPECT_TRUE(module.startfunc);
    EXPECT_EQ(*module.startfunc, 1);
}

TEST(parser, start_invalid_index)
{
    const auto func_section = make_vec({"00"_bytes, "00"_bytes});
    const auto start_section = "02"_bytes;
    const auto bin =
        bytes{wasm_prefix} + make_section(3, func_section) + make_section(8, start_section);

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "invalid start function index");
}

TEST(parser, start_missing_funcsec)
{
    const auto start_section = "01"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(8, start_section);

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "invalid start function index");
}

TEST(parser, start_module_with_imports)
{
    const auto import_section =
        make_vec({bytes{0x03, 'm', 'o', 'd', 0x03, 'f', 'o', 'o', 0x00, 0x42}});
    const auto func_section = make_vec({"00"_bytes, "00"_bytes});
    const auto start_section = "02"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(2, import_section) +
                     make_section(3, func_section) + make_section(8, start_section);

    const auto module = parse(bin);
    EXPECT_TRUE(module.startfunc);
    EXPECT_EQ(*module.startfunc, 2);
}

TEST(parser, start_module_with_imports_invalid_index)
{
    const auto import_section =
        make_vec({bytes{0x03, 'm', 'o', 'd', 0x03, 'f', 'o', 'o', 0x00, 0x42}});
    const auto func_section = make_vec({"00"_bytes, "00"_bytes});
    const auto start_section = "03"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(2, import_section) +
                     make_section(3, func_section) + make_section(8, start_section);

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "invalid start function index");
}

TEST(parser, start_index_decode_out_of_bounds)
{
    const auto wasm = bytes{wasm_prefix} + make_section(8, "ff"_bytes);
    EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "Unexpected EOF");
}

TEST(parser, element_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(9, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.elementsec.size(), 0);
}

TEST(parser, element_section)
{
    const auto table_contents = bytes{0x01, 0x70, 0x00, 0x7f};
    const auto element_contents = make_vec({bytes{0x00, 0x41, 0x01, 0x0b, 0x02, 0x7f, 0x7f},
        bytes{0x00, 0x41, 0x02, 0x0b, 0x02, 0x55, 0x55},
        bytes{0x00, 0x23, 0x00, 0x0b, 0x02, 0x24, 0x24}});
    const auto bin =
        bytes{wasm_prefix} + make_section(4, table_contents) + make_section(9, element_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.elementsec.size(), 3);
    EXPECT_EQ(module.elementsec[0].offset.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module.elementsec[0].offset.value.constant, 1);
    ASSERT_EQ(module.elementsec[0].init.size(), 2);
    EXPECT_EQ(module.elementsec[0].init[0], 0x7f);
    EXPECT_EQ(module.elementsec[0].init[1], 0x7f);
    EXPECT_EQ(module.elementsec[1].offset.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module.elementsec[1].offset.value.constant, 2);
    ASSERT_EQ(module.elementsec[1].init.size(), 2);
    EXPECT_EQ(module.elementsec[1].init[0], 0x55);
    EXPECT_EQ(module.elementsec[1].init[1], 0x55);
    EXPECT_EQ(module.elementsec[2].offset.kind, ConstantExpression::Kind::GlobalGet);
    EXPECT_EQ(module.elementsec[2].offset.value.global_index, 0);
    ASSERT_EQ(module.elementsec[2].init.size(), 2);
    EXPECT_EQ(module.elementsec[2].init[0], 0x24);
    EXPECT_EQ(module.elementsec[2].init[1], 0x24);
}

TEST(parser, element_section_tableidx_nonzero)
{
    const auto section_contents = bytes{0x01, 0x01, 0x41, 0x01, 0x0b, 0x01, 0x00};
    const auto bin = bytes{wasm_prefix} + make_section(9, section_contents);

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "unexpected tableidx value 1");
}

TEST(parser, element_section_no_table_section)
{
    const auto wasm =
        bytes{wasm_prefix} + make_section(9, make_vec({"000b"_bytes + make_vec({"00"_bytes})}));
    EXPECT_THROW_MESSAGE(
        parse(wasm), parser_error, "element section encountered without a table section");
}

TEST(parser, code_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(10, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, code_locals)
{
    const auto wasm_locals = "81017f"_bytes;  // 0x81 x i32.
    const auto wasm =
        bytes{wasm_prefix} +
        make_section(10, make_vec({add_size_prefix(make_vec({wasm_locals}) + "0b"_bytes)}));

    const auto module = parse(wasm);
    ASSERT_EQ(module.codesec.size(), 1);
    EXPECT_EQ(module.codesec[0].local_count, 0x81);
}

TEST(parser, code_locals_2)
{
    const auto wasm_locals1 = "017e"_bytes;  // 1 x i64.
    const auto wasm_locals2 = "027f"_bytes;  // 2 x i32.
    const auto wasm_locals3 = "037e"_bytes;  // 3 x i64.
    const auto wasm_locals4 = "047e"_bytes;  // 4 x i64.
    const auto wasm =
        bytes{wasm_prefix} +
        make_section(10,
            make_vec({add_size_prefix(
                make_vec({wasm_locals1, wasm_locals2, wasm_locals3, wasm_locals4}) + "0b"_bytes)}));

    const auto module = parse(wasm);
    ASSERT_EQ(module.codesec.size(), 1);
    EXPECT_EQ(module.codesec[0].local_count, 1 + 2 + 3 + 4);
}

TEST(parser, code_locals_invalid_type)
{
    const auto wasm_locals = "017b"_bytes;  // 1 x <invalid_type>.
    const auto wasm =
        bytes{wasm_prefix} +
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
        const auto wasm =
            bytes{wasm_prefix} + make_section(10, make_vec({add_size_prefix(locals + "0b"_bytes)}));

        EXPECT_THROW_MESSAGE(parse(wasm), parser_error, "too many local variables");
    }
}

TEST(parser, code_with_empty_expr_2_locals)
{
    // Func with 2x i32 locals, only 0x0b "end" instruction.
    const auto func_2_locals_bin = "01027f0b"_bytes;
    const auto code_bin = add_size_prefix(func_2_locals_bin);
    const auto wasm_bin = bytes{wasm_prefix} + make_section(10, make_vec({code_bin}));

    const auto module = parse(wasm_bin);
    ASSERT_EQ(module.codesec.size(), 1);
    const auto& code_obj = module.codesec[0];
    EXPECT_EQ(code_obj.local_count, 2);
    ASSERT_EQ(code_obj.instructions.size(), 1);
    EXPECT_EQ(code_obj.instructions[0], Instr::end);
    EXPECT_EQ(code_obj.immediates.size(), 0);
}

TEST(parser, code_with_empty_expr_5_locals)
{
    // Func with 1x i64 + 4x i32 locals , only 0x0b "end" instruction.
    const auto func_5_locals_bin = "02017f047e0b"_bytes;
    const auto code_bin = add_size_prefix(func_5_locals_bin);
    const auto wasm_bin = bytes{wasm_prefix} + make_section(10, make_vec({code_bin}));

    const auto module = parse(wasm_bin);
    ASSERT_EQ(module.codesec.size(), 1);
    const auto& code_obj = module.codesec[0];
    EXPECT_EQ(code_obj.local_count, 5);
    ASSERT_EQ(code_obj.instructions.size(), 1);
    EXPECT_EQ(code_obj.instructions[0], Instr::end);
    EXPECT_EQ(code_obj.immediates.size(), 0);
}

TEST(parser, code_section_with_2_trivial_codes)
{
    const auto func_nolocals_bin = "000b"_bytes;
    const auto code_bin = add_size_prefix(func_nolocals_bin);
    const auto section_contents = make_vec({code_bin, code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(10, section_contents);

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
    const auto func_bin =
        "00"  // vec(locals)
        "2001210222036a01000b"_bytes;
    const auto code_bin = add_size_prefix(func_bin);
    const auto section_contents = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(10, section_contents);

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
    EXPECT_EQ(module.codesec[0].immediates, "010000000200000003000000"_bytes);
}

TEST(parser, code_section_with_memory_size)
{
    const auto func_bin =
        "00"  // vec(locals)
        "3f000b"_bytes;
    const auto code_bin = add_size_prefix(func_bin);
    const auto section_contents = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(10, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.codesec.size(), 1);
    EXPECT_EQ(module.codesec[0].local_count, 0);
    ASSERT_EQ(module.codesec[0].instructions.size(), 2);
    EXPECT_EQ(module.codesec[0].instructions[0], Instr::memory_size);
    EXPECT_EQ(module.codesec[0].instructions[1], Instr::end);
    EXPECT_TRUE(module.codesec[0].immediates.empty());

    const auto func_bin_invalid =
        "00"  // vec(locals)
        "3f010b"_bytes;
    const auto code_bin_invalid = add_size_prefix(func_bin_invalid);
    const auto section_contents_invalid = make_vec({code_bin_invalid});
    const auto bin_invalid = bytes{wasm_prefix} + make_section(10, section_contents_invalid);

    EXPECT_THROW_MESSAGE(parse(bin_invalid), parser_error, "invalid memory index encountered");
}

TEST(parser, code_section_with_memory_grow)
{
    const auto func_bin =
        "00"  // vec(locals)
        "410040001a0b"_bytes;
    const auto code_bin = add_size_prefix(func_bin);
    const auto section_contents = make_vec({code_bin});
    const auto bin = bytes{wasm_prefix} + make_section(10, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.codesec.size(), 1);
    EXPECT_EQ(module.codesec[0].local_count, 0);
    ASSERT_EQ(module.codesec[0].instructions.size(), 4);
    EXPECT_EQ(module.codesec[0].instructions[0], Instr::i32_const);
    EXPECT_EQ(module.codesec[0].instructions[1], Instr::memory_grow);
    EXPECT_EQ(module.codesec[0].instructions[2], Instr::drop);
    EXPECT_EQ(module.codesec[0].instructions[3], Instr::end);
    EXPECT_EQ(module.codesec[0].immediates, "00000000"_bytes);

    const auto func_bin_invalid =
        "00"  // vec(locals)
        "410040011a0b"_bytes;
    const auto code_bin_invalid = add_size_prefix(func_bin_invalid);
    const auto section_contents_invalid = make_vec({code_bin_invalid});
    const auto bin_invalid = bytes{wasm_prefix} + make_section(10, section_contents_invalid);

    EXPECT_THROW_MESSAGE(parse(bin_invalid), parser_error, "invalid memory index encountered");
}

TEST(parser, code_section_unsupported_fp_instructions)
{
    const uint8_t fp_instructions[] = {0x2a, 0x2b, 0x38, 0x39, 0x43, 0x44, 0x5b, 0x5c, 0x5d, 0x5e,
        0x5f, 0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f, 0x90, 0x91,
        0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f, 0xa0,
        0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa6, 0xa8, 0xa9, 0xaa, 0xab, 0xae, 0xaf, 0xb0, 0xb1, 0xb2,
        0xb3, 0xb4, 0xb5, 0xb6, 0xb7, 0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbe, 0xbf};

    for (const auto instr : fp_instructions)
    {
        const auto func_bin = "00"_bytes  // vec(locals)
                              + bytes{instr};
        const auto code_bin = add_size_prefix(func_bin);
        const auto section_contents = make_vec({code_bin});
        const auto bin = bytes{wasm_prefix} + make_section(10, section_contents);

        const auto expected_msg =
            std::string{"unsupported floating point instruction "} + std::to_string(instr);
        EXPECT_THROW_MESSAGE(parse(bin), parser_error, expected_msg.c_str());
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
        const auto bin = bytes{wasm_prefix} + make_section(10, section_contents);

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

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "malformed size field for function");
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

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "malformed size field for function");
}

TEST(parser, data_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(11, make_vec({}));
    const auto module = parse(bin);
    EXPECT_EQ(module.datasec.size(), 0);
}

TEST(parser, data_section)
{
    const auto section_contents =
        make_vec({"0041010b02aaff"_bytes, "0041020b025555"_bytes, "0023000b022424"_bytes});
    const auto bin = bytes{wasm_prefix} + make_section(11, section_contents);

    const auto module = parse(bin);
    ASSERT_EQ(module.datasec.size(), 3);
    EXPECT_EQ(module.datasec[0].offset.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module.datasec[0].offset.value.constant, 1);
    EXPECT_EQ(module.datasec[0].init, "aaff"_bytes);
    EXPECT_EQ(module.datasec[1].offset.kind, ConstantExpression::Kind::Constant);
    EXPECT_EQ(module.datasec[1].offset.value.constant, 2);
    EXPECT_EQ(module.datasec[1].init, "5555"_bytes);
    EXPECT_EQ(module.datasec[2].offset.kind, ConstantExpression::Kind::GlobalGet);
    EXPECT_EQ(module.datasec[2].offset.value.global_index, 0);
    EXPECT_EQ(module.datasec[2].init, "2424"_bytes);
}

TEST(parser, data_section_memidx_nonzero)
{
    const auto section_contents = make_vec({"0141010b0100"_bytes});
    const auto bin = bytes{wasm_prefix} + make_section(11, section_contents);

    EXPECT_THROW_MESSAGE(parse(bin), parser_error, "unexpected memidx value 1");
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
    const auto type_section = make_vec({functype_void_to_void});
    const auto func_section = make_vec({"00"_bytes});
    const auto code_section = make_vec({add_size_prefix("000b"_bytes)});
    const auto bin = bytes{wasm_prefix} + make_section(0, "0161"_bytes) +
                     make_section(1, type_section) + make_section(0, "0162"_bytes) +
                     make_section(3, func_section) + make_section(0, "0163"_bytes) +
                     make_section(10, code_section);

    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 1);
    EXPECT_EQ(module.funcsec.size(), 1);
    EXPECT_EQ(module.codesec.size(), 1);
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

    ASSERT_EQ(m.typesec.size(), 1);
    EXPECT_EQ(m.typesec[0].inputs, (std::vector{ValType::i32, ValType::i32}));
    EXPECT_EQ(m.typesec[0].outputs, (std::vector{ValType::i32}));

    ASSERT_EQ(m.codesec.size(), 1);
    const auto& c = m.codesec[0];
    EXPECT_EQ(c.local_count, 1);
    EXPECT_EQ(c.instructions,
        (std::vector{Instr::local_get, Instr::local_get, Instr::i32_add, Instr::local_get,
            Instr::i32_add, Instr::local_tee, Instr::local_get, Instr::i32_add, Instr::end}));
    EXPECT_EQ(c.immediates,
        "00000000"
        "01000000"
        "02000000"
        "02000000"
        "00000000"_bytes);
}
