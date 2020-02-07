#include "parser.hpp"
#include <gtest/gtest.h>
#include <lib/fizzy/types.hpp>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>
#include <types.hpp>

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
    const auto input = "037f7e7fcc"_bytes;
    const auto [vec, pos] = parser<std::vector<ValType>>{}(input.data());
    EXPECT_EQ(pos, input.data() + 4);
    ASSERT_EQ(vec.size(), 3);
    EXPECT_EQ(vec[0], ValType::i32);
    EXPECT_EQ(vec[1], ValType::i64);
    EXPECT_EQ(vec[2], ValType::i32);
}

TEST(parser, limits_min)
{
    const auto input = "007f"_bytes;
    const auto [limits, pos] = parser<Limits>{}(input.data());
    EXPECT_EQ(limits.min, 0x7f);
    EXPECT_FALSE(limits.max.has_value());
}

TEST(parser, limits_minmax)
{
    const auto input = "01207f"_bytes;
    const auto [limits, pos] = parser<Limits>{}(input.data());
    EXPECT_EQ(limits.min, 0x20);
    EXPECT_TRUE(limits.max.has_value());
    EXPECT_EQ(*limits.max, 0x7f);
}

TEST(parser, DISABLED_limits_min_invalid_too_short)
{
    const auto input = "00"_bytes;
    EXPECT_THROW(parser<Limits>{}(input.data()), parser_error);
}

TEST(parser, DISABLED_limits_minmax_invalid_too_short)
{
    const auto input = "0120"_bytes;
    EXPECT_THROW(parser<Limits>{}(input.data()), parser_error);
}

TEST(parser, limits_invalid)
{
    const auto input = "02"_bytes;
    EXPECT_THROW(parser<Limits>{}(input.data()), parser_error);
}

TEST(parser, locals)
{
    const auto input = "81017f"_bytes;
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
    EXPECT_THROW(parse("006173d6"_bytes), parser_error);
    EXPECT_THROW(parse("006173d600000000"_bytes), parser_error);
    EXPECT_THROW(parse("006173d602000000"_bytes), parser_error);
}

TEST(parser, custom_section_empty)
{
    const auto bin = bytes{wasm_prefix} + make_section(0, bytes{});
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, custom_section_nonempty)
{
    const auto bin = bytes{wasm_prefix} + make_section(0, "ff"_bytes);
    const auto module = parse(bin);
    EXPECT_EQ(module.typesec.size(), 0);
    EXPECT_EQ(module.funcsec.size(), 0);
    EXPECT_EQ(module.codesec.size(), 0);
}

TEST(parser, functype_wrong_prefix)
{
    const auto section_contents =
        "01"
        "610000"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(1, section_contents);
    EXPECT_THROW(parse(bin), parser_error);
}

TEST(parser, type_section_larger_than_expected)
{
    const auto section_contents = "01"_bytes + functype_void_to_void;
    const auto bin =
        bytes{wasm_prefix} +
        make_invalid_size_section(1, size_t{section_contents.size() - 1}, section_contents);
    EXPECT_THROW(parse(bin), parser_error);
}

TEST(parser, type_section_smaller_than_expected)
{
    const auto section_contents = "01"_bytes + functype_void_to_void + "fe"_bytes;
    const auto bin =
        bytes{wasm_prefix} +
        make_invalid_size_section(1, size_t{section_contents.size() + 1}, section_contents);
    EXPECT_THROW(parse(bin), parser_error);
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

    EXPECT_THROW(parse(bin), parser_error);
}

TEST(parser, table_multi_min_limit)
{
    const auto section_contents = "0270007f70007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    EXPECT_THROW(parse(bin), parser_error);
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

    EXPECT_THROW(parse(bin), parser_error);
}

TEST(parser, memory_multi_min_limit)
{
    const auto section_contents = "02007f007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    EXPECT_THROW(parse(bin), parser_error);
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

TEST(parser, code_with_empty_expr_2_locals)
{
    // Func with 2x i32 locals, only 0x0b "end" instruction.
    const auto func_2_locals_bin = "01027f0b"_bytes;
    const auto code_bin = add_size_prefix(func_2_locals_bin);

    const auto [code_obj, end_pos1] = parser<Code>{}(code_bin.data());
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

    const auto [code_obj, end_pos1] = parser<Code>{}(code_bin.data());
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

    const auto bin =
        "0061736d0100000001070160027f7f017f030201000a13011101017f200020016a20026a220220006a0b"_bytes;
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
    EXPECT_EQ(c.immediates,
        "00000000"
        "01000000"
        "02000000"
        "02000000"
        "00000000"_bytes);
}

TEST(parser, instr_loop)
{
    const auto loop_void_empty = "03400b0b"_bytes;
    const auto [code1, pos1] = parse_expr(loop_void_empty.data());
    EXPECT_EQ(code1.instructions, (std::vector{Instr::loop, Instr::end, Instr::end}));
    EXPECT_EQ(code1.immediates.size(), 0);

    const auto loop_i32_empty = "037f0b0b"_bytes;
    const auto [code2, pos2] = parse_expr(loop_i32_empty.data());
    EXPECT_EQ(code2.instructions, (std::vector{Instr::loop, Instr::end, Instr::end}));
    EXPECT_EQ(code2.immediates.size(), 0);
}

TEST(parser, DISABLED_instr_loop_input_buffer_overflow)
{
    // The function end opcode 0b is missing causing reading out of input buffer.
    const auto loop_missing_end = "03400b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(loop_missing_end.data()), parser_error, "???");
}

TEST(parser, instr_block)
{
    const auto wrong_type = "0200"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(wrong_type.data()), parser_error, "invalid valtype 0");

    const auto empty = "010102400b0b"_bytes;
    const auto [code1, pos1] = parse_expr(empty.data());
    EXPECT_EQ(code1.instructions,
        (std::vector{Instr::nop, Instr::nop, Instr::block, Instr::end, Instr::end}));
    EXPECT_EQ(code1.immediates,
        "00"
        "04000000"
        "09000000"_bytes);

    const auto block_i64 = "027e0b0b"_bytes;
    const auto [code2, pos2] = parse_expr(block_i64.data());
    EXPECT_EQ(code2.instructions, (std::vector{Instr::block, Instr::end, Instr::end}));
    EXPECT_EQ(code2.immediates,
        "01"
        "02000000"
        "09000000"_bytes);
}

TEST(parser, DISABLED_instr_block_input_buffer_overflow)
{
    // The function end opcode 0b is missing causing reading out of input buffer.
    const auto block_missing_end = "02400b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(block_missing_end.data()), parser_error, "???");
}

TEST(parser, block_br)
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
    const auto [code, pos] = parse_expr(code_bin.data());
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
}

TEST(parser, instr_br_table)
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

    const auto [code, pos] = parse_expr(code_bin.data());

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
}

TEST(parser, instr_br_table_empty_vector)
{
    /*
      (block
        (br_table 0 (get_local 0))
        (return (i32.const 99))
      )
      (i32.const 100)
    */

    const auto code_bin = "024020000e000041e3000f0b41e4000b000c046e616d6502050100010000"_bytes;

    const auto [code, pos] = parse_expr(code_bin.data());

    EXPECT_EQ(code.instructions,
        (std::vector{Instr::block, Instr::local_get, Instr::br_table, Instr::i32_const,
            Instr::return_, Instr::end, Instr::i32_const, Instr::end}));

    // blocks + local_get before br_table
    const auto br_table_imm_offset = 1 + 2 * 4 + 4;
    const auto expected_br_imm =
        "00000000"
        "00000000"_bytes;
    EXPECT_EQ(code.immediates.substr(br_table_imm_offset, expected_br_imm.size()), expected_br_imm);
}

TEST(parser, unexpected_else)
{
    // (else)
    const auto code1_bin = "050b0b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(code1_bin.data()), parser_error, "unexpected else instruction");

    // (block (else))
    const auto code2_bin = "0240050b0b0b"_bytes;
    EXPECT_THROW_MESSAGE(parse_expr(code2_bin.data()), parser_error,
        "unexpected else instruction (if instruction missing)");
}
