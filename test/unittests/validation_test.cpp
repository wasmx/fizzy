// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>
#include <test/utils/wasm_binary.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(validation, function_type_invalid_arity)
{
    /* wat2wasm --no-check
    (type (func (result i32 i32)))
    */
    const auto wasm = from_hex("0061736d010000000106016000027f7f");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "function has more than one result");
}

TEST(validation, imported_function_unknown_type)
{
    /* wat2wasm --no-check
    (type (func (result i32)))
    (func (import "m" "f") (type 1))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020701016d01660001");
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "invalid type index of an imported function");
}

TEST(validation, globals_invalid_initializer)
{
    // initializing from non-existing global
    /* wat2wasm --no-check
      (global i32 (global.get 1))
    */
    const auto bin_invalid1 = from_hex("0061736d010000000606017f0023010b");
    EXPECT_THROW_MESSAGE(
        parse(bin_invalid1), validation_error, "invalid global index in constant expression");

    // initializing from mutable global is not allowed
    /* wat2wasm --no-check
      (global (import "mod" "g1") (mut i32))
      (global (mut i32) (global.get 0))
    */
    const auto bin_invalid2 =
        from_hex("0061736d01000000020b01036d6f64026731037f010606017f0123000b");
    EXPECT_THROW_MESSAGE(parse(bin_invalid2), validation_error,
        "constant expression can use global.get only for const globals");

    // initializing from non-imported global is not allowed
    /* wat2wasm --no-check
      (global i32 (i32.const 42))
      (global (mut i32) (global.get 0))
    */
    const auto bin_invalid3 = from_hex("0061736d01000000060b027f00412a0b7f0123000b");
    EXPECT_THROW_MESSAGE(parse(bin_invalid3), validation_error,
        "global can be initialized by another const global only if it's imported");

    /* wat2wasm --no-check
      (global (mut i32) (i32.const 16) (i32.const -1))
    */
    const auto bin_multi = from_hex("0061736d010000000608017f014110417f0b");
    EXPECT_THROW_MESSAGE(
        parse(bin_multi), validation_error, "constant expression has multiple instructions");

    /* wat2wasm --no-check
      (global (mut i64))
    */
    const auto bin_no_instr = from_hex("0061736d010000000604017e010b");
    EXPECT_THROW_MESSAGE(parse(bin_no_instr), validation_error, "constant expression is empty");

    /* wat2wasm --no-check
      (global (mut i32) (i64.const 16))
    */
    const auto bin_const_type_mismatch = from_hex("0061736d010000000606017f0142100b");
    EXPECT_THROW_MESSAGE(
        parse(bin_const_type_mismatch), validation_error, "constant expression type mismatch");

    /* wat2wasm --no-check
      (global (import "mod" "g1") i64)
      (global i32 (global.get 0))
    */
    const auto bin_global_type_mismatch =
        from_hex("0061736d01000000020b01036d6f64026731037e000606017f0023000b");
    EXPECT_THROW_MESSAGE(
        parse(bin_global_type_mismatch), validation_error, "constant expression type mismatch");
}

TEST(validation, import_memories_multiple)
{
    const auto section_contents =
        make_vec({bytes{0x02, 'm', '1', 0x03, 'a', 'b', 'c', 0x02, 0x00, 0x7f},
            bytes{0x02, 'm', '2', 0x03, 'd', 'e', 'f', 0x02, 0x00, 0x7f}});
    const auto bin = bytes{wasm_prefix} + make_section(2, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "too many imported memories (at most one is allowed)");
}

TEST(validation, memory_and_imported_memory)
{
    // (import "js" "mem"(memory 1))
    const auto import_section = "020b01026a73036d656d0200010008046e616d65020100"_bytes;
    // (memory 1)
    const auto memory_section = "05030100010008046e616d65020100"_bytes;
    const auto bin = bytes{wasm_prefix} + import_section + memory_section;

    EXPECT_THROW_MESSAGE(parse(bin), validation_error,
        "both module memory and imported memory are defined (at most one of them is allowed)");
}

TEST(validation, memory_multi_min_limit)
{
    const auto section_contents = "02007f007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "too many memory sections (at most one is allowed)");
}

TEST(validation, import_tables_multiple)
{
    const auto section_contents =
        make_vec({bytes{0x02, 'm', '1', 0x03, 'a', 'b', 'c', 0x01, 0x70, 0x00, 0x01},
            bytes{0x02, 'm', '2', 0x03, 'd', 'e', 'f', 0x01, 0x70, 0x01, 0x01, 0x03}});
    const auto bin = bytes{wasm_prefix} + make_section(2, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "too many imported tables (at most one is allowed)");
}

TEST(validation, table_and_imported_table)
{
    // (import "js" "t" (table 1 anyfunc))
    const auto import_section = "020a01026a730174017000010008046e616d65020100"_bytes;
    // (table 2 anyfunc)
    const auto table_section = "0404017000020008046e616d65020100"_bytes;
    const auto bin = bytes{wasm_prefix} + import_section + table_section;

    EXPECT_THROW_MESSAGE(parse(bin), validation_error,
        "both module table and imported table are defined (at most one of them is allowed)");
}

TEST(validation, table_multi_min_limit)
{
    const auto section_contents = "0270007f70007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "too many table sections (at most one is allowed)");
}

TEST(validation, data_section_invalid_offset_expression)
{
    /* wat2wasm --no-check
      (memory 1 1)
      (data (global.get 0) "\aa\ff")
    */
    const auto bin1 = from_hex("0061736d010000000504010101010b08010023000b02aaff");

    EXPECT_THROW_MESSAGE(
        parse(bin1), validation_error, "invalid global index in constant expression");

    /* wat2wasm --no-check
      (global (mut i32) (i32.const 42))
      (memory 1 1)
      (data (global.get 0) "\aa\ff")
    */
    const auto bin2 = from_hex("0061736d010000000504010101010606017f01412a0b0b08010023000b02aaff");

    EXPECT_THROW_MESSAGE(parse(bin2), validation_error,
        "constant expression can use global.get only for const globals");

    /* wat2wasm --no-check
      (memory 1 1)
      (global i64 (i64.const 42))
      (data (global.get 0) "\aa\ff")
    */
    const auto bin_const_type_mismatch =
        from_hex("0061736d010000000504010101010606017e00422a0b0b08010023000b02aaff");
    EXPECT_THROW_MESSAGE(
        parse(bin_const_type_mismatch), validation_error, "constant expression type mismatch");

    /* wat2wasm --no-check
      (memory 1 1)
      (data (i64.const 0) "\aa\ff")
    */
    const auto bin_global_type_mismatch =
        from_hex("0061736d010000000504010101010b08010042000b02aaff");
    EXPECT_THROW_MESSAGE(
        parse(bin_global_type_mismatch), validation_error, "constant expression type mismatch");
}

TEST(validation, element_section_invalid_offset_expression)
{
    /* wat2wasm --no-check
      (table 4 funcref)
      (elem (global.get 0) 0 1)
      (func)
      (func)
    */
    const auto bin1 = from_hex(
        "0061736d0100000001040160000003030200000404017000040908010023000b0200010a070202000b02000b");

    EXPECT_THROW_MESSAGE(
        parse(bin1), validation_error, "invalid global index in constant expression");

    /* wat2wasm --no-check
      (global (mut i32) (i32.const 42))
      (table 4 funcref)
      (elem (global.get 0) 0 1)
      (func)
      (func)
    */
    const auto bin2 = from_hex(
        "0061736d0100000001040160000003030200000404017000040606017f01412a0b0908010023000b0200010a07"
        "0202000b02000b");

    EXPECT_THROW_MESSAGE(parse(bin2), validation_error,
        "constant expression can use global.get only for const globals");

    /* wat2wasm --no-check
      (global i64 (i64.const 42))
      (table 4 funcref)
      (elem (global.get 0) 0 1)
      (func)
      (func)
    */
    const auto bin_const_type_mismatch = from_hex(
        "0061736d0100000001040160000003030200000404017000040606017e00422a0b0908010023000b0200010a07"
        "0202000b02000b");
    EXPECT_THROW_MESSAGE(
        parse(bin_const_type_mismatch), validation_error, "constant expression type mismatch");

    /* wat2wasm --no-check
      (table 4 funcref)
      (elem (i64.const 0) 0 1)
      (func)
      (func)
    */
    const auto bin_global_type_mismatch = from_hex(
        "0061736d0100000001040160000003030200000404017000040908010042000b0200010a070202000b02000b");
    EXPECT_THROW_MESSAGE(
        parse(bin_global_type_mismatch), validation_error, "constant expression type mismatch");
}

TEST(validation, element_section_invalid_function_index)
{
    /* wat2wasm --no-check
      (table 4 funcref)
      (elem (i32.const 0) 0)
    */
    const auto bin1 = from_hex("0061736d010000000404017000040907010041000b0100");
    EXPECT_THROW_MESSAGE(
        parse(bin1), validation_error, "invalid function index in element section");

    /* wat2wasm --no-check
      (table 4 funcref)
      (elem (i32.const 0) 0 1)
      (func)
    */
    const auto bin2 = from_hex(
        "0061736d01000000010401600000030201000404017000040908010041000b0200010a040102000b");
    EXPECT_THROW_MESSAGE(
        parse(bin2), validation_error, "invalid function index in element section");

    /* wat2wasm --no-check
      (table 4 funcref)
      (elem (i32.const 0) 0)
      (elem (i32.const 1) 1)
      (func)
    */
    const auto bin3 = from_hex(
        "0061736d0100000001040160000003020100040401700004090d020041000b01000041010b01010a040102000"
        "b");
    EXPECT_THROW_MESSAGE(
        parse(bin3), validation_error, "invalid function index in element section");

    /* wat2wasm --no-check
      (type (func (result i32)))
      (func (import "m" "f") (type 0))
      (table 4 funcref)
      (elem (i32.const 0) 0 1)
    */
    const auto bin4 = from_hex(
        "0061736d010000000105016000017f020701016d016600000404017000040908010041000b020001");
    EXPECT_THROW_MESSAGE(
        parse(bin4), validation_error, "invalid function index in element section");
}

TEST(validation, i32_store_no_memory)
{
    /* wat2wasm --no-check
    (func (param i32)
      get_local 0
      i32.const 0
      i32.store
    )
    */
    const auto wasm = from_hex("0061736d0100000001050160017f00030201000a0b010900200041003602000b");
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "memory instructions require imported or defined memory");
}

TEST(validation, f32_store_no_memory)
{
    /* wat2wasm --no-check
    (func (param i32)
      get_local 0
      f32.const 0
      f32.store
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001050160017f00030201000a0e010c00200043000000003802000b");
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "memory instructions require imported or defined memory");
}

TEST(validation, memory_size_no_memory)
{
    /* wat2wasm --no-check
    (func (result i32)
      memory.size
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a060104003f000b");
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "memory instructions require imported or defined memory");
}

TEST(validation, store_alignment)
{
    // NOTE: could use instruction_metrics here, but better to have two sources of truth for testing
    const std::tuple<Instr, int, Instr> test_cases[] = {{Instr::i32_store8, 0, Instr::i32_const},
        {Instr::i32_store16, 1, Instr::i32_const}, {Instr::i32_store, 2, Instr::i32_const},
        {Instr::i64_store8, 0, Instr::i64_const}, {Instr::i64_store16, 1, Instr::i64_const},
        {Instr::i64_store32, 2, Instr::i64_const}, {Instr::i64_store, 3, Instr::i64_const},
        {Instr::f32_store, 2, Instr::f32_const}, {Instr::f64_store, 3, Instr::f64_const}};

    for (const auto& test_case : test_cases)
    {
        const auto [instr, max_align, push_address_instr] = test_case;
        // TODO: consider using leb128_encode and test 2^32-1
        for (auto align : {0, 1, 2, 3, 4, 0x7f})
        {
            /*
            (func (param i32)
              get_local 0
              i32.const 0
              <instr> align=<align>
            */
            const auto type_section = make_vec({"60017f00"_bytes});
            const auto function_section = make_vec({"00"_bytes});
            // NOTE: this depends on align < 0x80
            auto code_bin = bytes{0,  // vec(locals)
                uint8_t(Instr::local_get), 0, uint8_t(push_address_instr)};
            if (push_address_instr == Instr::f32_const)
                code_bin += "00000000"_bytes;
            else if (push_address_instr == Instr::f64_const)
                code_bin += "0000000000000000"_bytes;
            else
                code_bin += "00"_bytes;
            code_bin += bytes{uint8_t(instr), uint8_t(align), 0, uint8_t(Instr::end)};
            const auto code_section = make_vec({add_size_prefix(code_bin)});
            const auto memory_section = "01007f"_bytes;
            const auto bin = bytes{wasm_prefix} + make_section(1, type_section) +
                             make_section(3, function_section) + make_section(5, memory_section) +
                             make_section(10, code_section);
            if (align <= max_align)
            {
                EXPECT_NO_THROW(parse(bin));
            }
            else
            {
                EXPECT_THROW_MESSAGE(
                    parse(bin), validation_error, "alignment cannot exceed operand size");
            }
        }
    }
}

TEST(validation, load_alignment)
{
    // NOTE: could use instruction_metrics here, but better to have two sources of truth for testing
    const std::map<Instr, int> test_cases{
        {Instr::i32_load8_s, 0},
        {Instr::i32_load8_u, 0},
        {Instr::i32_load16_s, 1},
        {Instr::i32_load16_u, 1},
        {Instr::i32_load, 2},
        {Instr::i64_load8_s, 0},
        {Instr::i64_load8_u, 0},
        {Instr::i64_load16_s, 1},
        {Instr::i64_load16_u, 1},
        {Instr::i64_load32_s, 2},
        {Instr::i64_load32_u, 2},
        {Instr::i64_load, 3},
        {Instr::f32_load, 2},
        {Instr::f64_load, 3},
    };

    for (const auto test_case : test_cases)
    {
        const auto instr = test_case.first;
        const auto max_align = test_case.second;
        // TODO: consider using leb128_encode and test 2^32-1
        for (auto align : {0, 1, 2, 3, 4, 0x7f})
        {
            /*
            (func (param i32)
              get_local 0
              <instr> align=<align>
            */
            const auto type_section = make_vec({"60017f00"_bytes});
            const auto function_section = make_vec({"00"_bytes});
            // NOTE: this depends on align < 0x80
            const auto code_bin = bytes{0,  // vec(locals)
                uint8_t(Instr::local_get), 0, uint8_t(instr), uint8_t(align), 0,
                uint8_t(Instr::drop), uint8_t(Instr::end)};
            const auto code_section = make_vec({add_size_prefix(code_bin)});
            const auto memory_section = "01007f"_bytes;
            const auto bin = bytes{wasm_prefix} + make_section(1, type_section) +
                             make_section(3, function_section) + make_section(5, memory_section) +
                             make_section(10, code_section);
            if (align <= max_align)
            {
                EXPECT_NO_THROW(parse(bin));
            }
            else
            {
                EXPECT_THROW_MESSAGE(
                    parse(bin), validation_error, "alignment cannot exceed operand size");
            }
        }
    }
}

TEST(validation, br_invalid_label_index)
{
    /* wat2wasm --no-check
    (func
      br 1
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a060104000c010b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid label index");
}

TEST(validation, br_if_invalid_label_index)
{
    /* wat2wasm --no-check
    (func
      (block
        (loop
           i32.const 0
           br_if 3
        )
      )
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a0e010c000240034041000d030b0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid label index");
}

TEST(validation, br_table_invalid_label_index)
{
    /* wat2wasm --no-check
    (func
      (block
        (block
          (block
            (block
              (block
                (br_table 0 1 2 3 4 5 6 0 (i32.const 0))
              )
            )
          )
        )
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a1f011d000240024002400240024041000e070001020304050600"
        "0b0b0b0b0b0b");

    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid label index");
}

TEST(validation, br_table_default_invalid_label_index)
{
    /* wat2wasm --no-check
    (func
      (block
        (br_table 0 1 0 1 0 1 0 1 2 (i32.const 0))
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a14011200024041000e080001000100010001020b0b");

    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid label index");
}

TEST(validation, br_table_invalid_type)
{
    /* wat2wasm --no-check
    (func  (param $x i32) (result i32)
      (block $a (result i32)
        (block $b
          local.get $x
          br_table $a $b
        )
        i32.const 2
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000a12011000027f024020000e0101000b41020b0b");

    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "br_table labels have inconsistent types");

    /* wat2wasm --no-check
    (func  (param $x i32)
      (block $a
        (block $b (result i32)
          i32.const 1
          local.get $x
          br_table $b $a
        )
        drop
      )
    )
    */
    const auto wasm2 = from_hex(
        "0061736d0100000001050160017f00030201000a130111000240027f410120000e0100010b1a0b0b");

    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "br_table labels have inconsistent types");

    /* wat2wasm --no-check
    (func (param $x i32) (result i32)
      (loop $a (result i32)
        (block $b (result i32)
          i32.const 1
          local.get $x
          br_table $b $a
        )
      )
    )
    */
    const auto wasm3 = from_hex(
        "0061736d0100000001060160017f017f030201000a12011000037f027f410120000e0100010b0b0b");

    EXPECT_THROW_MESSAGE(parse(wasm3), validation_error, "br_table labels have inconsistent types");

    /* wat2wasm --no-check
    (func (param $x i32) (result i32)
      (block $a (result i32)
        (loop $b (result i32)
          i32.const 1
          local.get $x
          br_table $b $a
        )
      )
    )
    */
    const auto wasm4 = from_hex(
        "0061736d0100000001060160017f017f030201000a12011000027f037f410120000e0100010b0b0b");

    EXPECT_THROW_MESSAGE(parse(wasm4), validation_error, "br_table labels have inconsistent types");

    /* wat2wasm --no-check
    (func (param $x i32)
      (block $a (result i32)
        (block $b (result i64)
          i64.const 1
          local.get $x
          br_table $a $b
        )
      )
      drop
    )
    */
    const auto wasm5 = from_hex(
        "0061736d0100000001050160017f00030201000a13011100027f027e420120000e0101000b0b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm5), validation_error, "br_table labels have inconsistent types");
}

TEST(validation, call_unknown_function)
{
    /* wat2wasm --no-check
    (func (import "m" "f"))
    (func (result i32) call 2)
    */
    const auto wasm =
        from_hex("0061736d010000000108026000006000017f020701016d01660000030201010a0601040010020b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid funcidx encountered with call");
}

TEST(validation, call_indirect_unknown_type)
{
    /* wat2wasm --no-check
    (table anyfunc (elem 0))
    (func (param i32)
      (call_indirect (type 1) (get_local 0))
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f0003020100040501700101010907010041000b01000a090107002000110100"
        "0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid type index with call_indirect");
}

TEST(validation, call_indirect_no_table)
{
    /* wat2wasm --no-check
    (func (param i32)
      (call_indirect (type 1) (get_local 0))
    )
    */
    const auto wasm = from_hex("0061736d0100000001050160017f00030201000a0901070020001101000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "call_indirect without defined table");
}

TEST(validation, export_invalid_index)
{
    /* wat2wasm --no-check
    (export "foo" (func 0))
    */
    const auto wasm_func = from_hex("0061736d0100000007070103666f6f0000");
    EXPECT_THROW_MESSAGE(
        parse(wasm_func), validation_error, "invalid index of an exported function");

    /* wat2wasm --no-check
    (export "g" (global 0))
    */
    const auto wasm_glob = from_hex("0061736d0100000007050101670300");
    EXPECT_THROW_MESSAGE(parse(wasm_glob), validation_error, "invalid index of an exported global");

    /* wat2wasm --no-check
    (export "t" (table 0))
    */
    const auto wasm_no_table = from_hex("0061736d0100000007050101740100");
    EXPECT_THROW_MESSAGE(
        parse(wasm_no_table), validation_error, "invalid index of an exported table");

    /* wat2wasm --no-check
    (table 1 funcref)
    (export "t" (table 1))
    */
    const auto wasm_table = from_hex("0061736d0100000004040170000107050101740101");
    EXPECT_THROW_MESSAGE(parse(wasm_table), validation_error, "invalid index of an exported table");

    /* wat2wasm --no-check
    (export "m" (memory 0))
    */
    const auto wasm_no_mem = from_hex("0061736d01000000070501016d0200");
    EXPECT_THROW_MESSAGE(
        parse(wasm_no_mem), validation_error, "invalid index of an exported memory");

    /* wat2wasm --no-check
    (memory 1)
    (export "m" (memory 1))
    */
    const auto wasm_mem = from_hex("0061736d010000000503010001070501016d0201");
    EXPECT_THROW_MESSAGE(parse(wasm_mem), validation_error, "invalid index of an exported memory");
}

TEST(validation, export_duplicate_name)
{
    /* wat2wasm --no-check
    (func (export "foo") (nop))
    (func (export "foo") (nop))
    */
    const auto wasm_func = from_hex(
        "0061736d010000000104016000000303020000070d0203666f6f000003666f6f00010a09020300010b0300010"
        "b");
    EXPECT_THROW_MESSAGE(parse(wasm_func), validation_error, "duplicate export name foo");

    /* wat2wasm --no-check
    (func (export "foo") (nop))
    (global (export "foo") i32 (i32.const 0))
    */
    const auto wasm_func_glob = from_hex(
        "0061736d01000000010401600000030201000606017f0041000b070d0203666f6f000003666f6f03000a050103"
        "00010b");
    EXPECT_THROW_MESSAGE(parse(wasm_func_glob), validation_error, "duplicate export name foo");
}

TEST(validation, global_get_invalid_index)
{
    /* wat2wasm --no-check
    (func (param i32)
      (get_global 0)
    )
    */
    const auto wasm = from_hex("0061736d0100000001050160017f00030201000a0601040023000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "accessing global with invalid index");

    /* wat2wasm --no-check
    (global i32 (i32.const 0))
    (func (param i32)
      (get_global 1)
    )
    */
    const auto wasm_global =
        from_hex("0061736d0100000001050160017f00030201000606017f0041000b0a0601040023010b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_global), validation_error, "accessing global with invalid index");

    /* wat2wasm --no-check
    (global (import "mod" "g") i32)
    (func (param i32)
      (get_global 1)
    )
    */
    const auto wasm_imp_global =
        from_hex("0061736d0100000001050160017f00020a01036d6f640167037f00030201000a0601040023010b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_imp_global), validation_error, "accessing global with invalid index");

    /* wat2wasm --no-check
    (global (import "mod" "g") i32)
    (global i32 (i32.const 0))
    (func (param i32)
      (get_global 2)
    )
    */
    const auto wasm_two_globals = from_hex(
        "0061736d0100000001050160017f00020a01036d6f640167037f00030201000606017f0041000b0a0601040023"
        "020b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_two_globals), validation_error, "accessing global with invalid index");
}

TEST(validation, global_set_invalid_index)
{
    /* wat2wasm --no-check
    (func (param i32)
      (i32.const 0)
      (set_global 0)
    )
    */
    const auto wasm = from_hex("0061736d0100000001050160017f00030201000a08010600410024000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "accessing global with invalid index");

    /* wat2wasm --no-check
    (global i32 (i32.const 0))
    (func (param i32)
      (i32.const 0)
      (set_global 1)
    )
    */
    const auto wasm_global =
        from_hex("0061736d0100000001050160017f00030201000606017f0041000b0a08010600410024010b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_global), validation_error, "accessing global with invalid index");

    /* wat2wasm --no-check
    (global (import "mod" "g") i32)
    (func (param i32)
      (i32.const 0)
      (set_global 1)
    )
    */
    const auto wasm_imp_global = from_hex(
        "0061736d0100000001050160017f00020a01036d6f640167037f00030201000a08010600410024010b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_imp_global), validation_error, "accessing global with invalid index");

    /* wat2wasm --no-check
    (global (import "mod" "g") i32)
    (global i32 (i32.const 0))
    (func (param i32)
      (i32.const 0)
      (set_global 2)
    )
    */
    const auto wasm_two_globals = from_hex(
        "0061736d0100000001050160017f00020a01036d6f640167037f00030201000606017f0041000b0a0801060041"
        "0024020b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_two_globals), validation_error, "accessing global with invalid index");
}

TEST(validation, global_set_immutable)
{
    /* wat2wasm --no-check
    (global i32 (i32.const 0))
    (func (param i32)
      (i32.const 0)
      (set_global 0)
    )
    */
    const auto wasm_global =
        from_hex("0061736d0100000001050160017f00030201000606017f0041000b0a08010600410024000b");
    EXPECT_THROW_MESSAGE(parse(wasm_global), validation_error, "trying to mutate immutable global");

    /* wat2wasm --no-check
    (global (import "mod" "g") i32)
    (func (param i32)
      (i32.const 0)
      (set_global 0)
    )
    */
    const auto wasm_imp_global = from_hex(
        "0061736d0100000001050160017f00020a01036d6f640167037f00030201000a08010600410024000b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_imp_global), validation_error, "trying to mutate immutable global");
}

TEST(validation, start_function_type)
{
    /* wat2wasm --no-check
    (func $start (param i32))
    (start $start)
    */
    const auto wasm_func_param =
        from_hex("0061736d0100000001050160017f00030201000801000a040102000b");
    EXPECT_THROW_MESSAGE(parse(wasm_func_param), validation_error, "invalid start function type");

    /* wat2wasm --no-check
    (func $start (param i32) (result i32) (return (i32.const 0)))
    (start $start)
    */
    const auto wasm_func_param_result =
        from_hex("0061736d0100000001060160017f017f030201000801000a0701050041000f0b");
    EXPECT_THROW_MESSAGE(
        parse(wasm_func_param_result), validation_error, "invalid start function type");

    /* wat2wasm --no-check
    (func $start (result i32) (return (i32.const 0)))
    (start $start)
    */
    const auto wasm_func_result =
        from_hex("0061736d010000000105016000017f030201000801000a0701050041000f0b");
    EXPECT_THROW_MESSAGE(parse(wasm_func_result), validation_error, "invalid start function type");
}

TEST(validation, get_local_out_of_bounds)
{
    /* wat2wasm --no-check
    (func (result i32)
      local.get 0
    )
    */
    const auto wasm1 = from_hex("0061736d010000000105016000017f030201000a0601040020000b");
    EXPECT_THROW_MESSAGE(parse(wasm1), validation_error, "invalid local index");

    /* wat2wasm --no-check
    (func (param i32) (result i32)
      local.get 1
    )
    */
    const auto wasm2 = from_hex("0061736d0100000001060160017f017f030201000a0601040020010b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "invalid local index");

    /* wat2wasm --no-check
    (func (result i32)
      (local i64)
      local.get 1
    )
    */
    const auto wasm3 = from_hex("0061736d010000000105016000017f030201000a08010601017e20010b");
    EXPECT_THROW_MESSAGE(parse(wasm3), validation_error, "invalid local index");

    /* wat2wasm --no-check
    (func (param i32) (param i64) (result i32)
      (local i64)
      (local i32)
      local.get 4
    )
    */
    const auto wasm4 =
        from_hex("0061736d0100000001070160027f7e017f030201000a0a010802017e017f20040b");
    EXPECT_THROW_MESSAGE(parse(wasm4), validation_error, "invalid local index");
}

TEST(validation, set_local_out_of_bounds)
{
    /* wat2wasm --no-check
    (func
      i32.const 0
      local.set 0
    )
    */
    const auto wasm1 = from_hex("0061736d01000000010401600000030201000a08010600410021000b");
    EXPECT_THROW_MESSAGE(parse(wasm1), validation_error, "invalid local index");

    /* wat2wasm --no-check
    (func (param i32)
      i32.const 0
      local.set 1
    )
    */
    const auto wasm2 = from_hex("0061736d0100000001050160017f00030201000a08010600410021010b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "invalid local index");

    /* wat2wasm --no-check
    (func
      (local i64)
      i32.const 0
      local.set 1
    )
    */
    const auto wasm3 = from_hex("0061736d01000000010401600000030201000a0a010801017e410021010b");
    EXPECT_THROW_MESSAGE(parse(wasm3), validation_error, "invalid local index");

    /* wat2wasm --no-check
    (func (param i32) (param i64)
      (local i64)
      (local i32)
      i32.const 0
      local.set 4
    )
    */
    const auto wasm4 =
        from_hex("0061736d0100000001060160027f7e00030201000a0c010a02017e017f410021040b");
    EXPECT_THROW_MESSAGE(parse(wasm4), validation_error, "invalid local index");
}

TEST(validation, tee_local_out_of_bounds)
{
    /* wat2wasm --no-check
    (func (result i32)
      i32.const 0
      local.tee 0
    )
    */
    const auto wasm1 = from_hex("0061736d010000000105016000017f030201000a08010600410022000b");
    EXPECT_THROW_MESSAGE(parse(wasm1), validation_error, "invalid local index");

    /* wat2wasm --no-check
    (func (param i32) (result i32)
      i32.const 0
      local.tee 1
    )
    */
    const auto wasm2 = from_hex("0061736d0100000001060160017f017f030201000a08010600410022010b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "invalid local index");

    /* wat2wasm --no-check
    (func (result i32)
      (local i64)
      i32.const 0
      local.tee 1
    )
    */
    const auto wasm3 = from_hex("0061736d010000000105016000017f030201000a0a010801017e410022010b");
    EXPECT_THROW_MESSAGE(parse(wasm3), validation_error, "invalid local index");

    /* wat2wasm --no-check
    (func (param i32) (param i64) (result i32)
      (local i64)
      (local i32)
      i32.const 0
      local.tee 4
    )
    */
    const auto wasm4 =
        from_hex("0061736d0100000001070160027f7e017f030201000a0c010a02017e017f410022040b");
    EXPECT_THROW_MESSAGE(parse(wasm4), validation_error, "invalid local index");
}
