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
    (func (param f32)
      get_local 0
      f32.const 0
      f32.store
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001050160017d00030201000a0e010c00200043000000003802000b");
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
