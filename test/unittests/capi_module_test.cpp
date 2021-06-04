// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy::test;

/// Represents an invalid/mocked pointer to a host function for tests without execution.
static constexpr FizzyExternalFn NullFn = nullptr;

TEST(capi_module, get_function_type)
{
    /* wat2wasm
      (func)
      (func (param i32 i32) (result i32) (i32.const 0))
      (func (param i64))
      (func (param f64) (result f32) (f32.const 0))
    */
    const auto wasm = from_hex(
        "0061736d0100000001130460000060027f7f017f60017e0060017c017d030504000102030a140402000b040041"
        "000b02000b070043000000000b");
    const auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    const auto type0 = fizzy_get_function_type(module, 0);
    EXPECT_EQ(type0.inputs_size, 0);
    EXPECT_EQ(type0.output, FizzyValueTypeVoid);

    const auto type1 = fizzy_get_function_type(module, 1);
    EXPECT_EQ(type1.inputs_size, 2);
    EXPECT_EQ(type1.inputs[0], FizzyValueTypeI32);
    EXPECT_EQ(type1.inputs[1], FizzyValueTypeI32);
    EXPECT_EQ(type1.output, FizzyValueTypeI32);

    const auto type2 = fizzy_get_function_type(module, 2);
    EXPECT_EQ(type2.inputs_size, 1);
    EXPECT_EQ(type2.inputs[0], FizzyValueTypeI64);
    EXPECT_EQ(type2.output, FizzyValueTypeVoid);

    const auto type3 = fizzy_get_function_type(module, 3);
    EXPECT_EQ(type3.inputs_size, 1);
    EXPECT_EQ(type3.inputs[0], FizzyValueTypeF64);
    EXPECT_EQ(type3.output, FizzyValueTypeF32);

    fizzy_free_module(module);
}

TEST(capi_module, has_table)
{
    /* wat2wasm
      (module)
    */
    const auto wasm_no_table = from_hex("0061736d01000000");
    const auto module_no_table = fizzy_parse(wasm_no_table.data(), wasm_no_table.size(), nullptr);
    ASSERT_NE(module_no_table, nullptr);

    EXPECT_FALSE(fizzy_module_has_table(module_no_table));

    fizzy_free_module(module_no_table);

    /* wat2wasm
      (table 0 anyfunc)
    */
    const auto wasm_table = from_hex("0061736d01000000040401700000");
    const auto module_table = fizzy_parse(wasm_table.data(), wasm_table.size(), nullptr);
    ASSERT_NE(module_table, nullptr);

    EXPECT_TRUE(fizzy_module_has_table(module_table));

    fizzy_free_module(module_table);

    /* wat2wasm
      (table (import "m" "t") 10 30 funcref)
    */
    const auto wasm_imported_table = from_hex("0061736d01000000020a01016d01740170010a1e");
    const auto module_imported_table =
        fizzy_parse(wasm_imported_table.data(), wasm_imported_table.size(), nullptr);
    ASSERT_NE(module_imported_table, nullptr);

    EXPECT_TRUE(fizzy_module_has_table(module_imported_table));

    fizzy_free_module(module_imported_table);
}

TEST(capi_module, has_memory)
{
    /* wat2wasm
      (module)
    */
    const auto wasm_no_memory = from_hex("0061736d01000000");
    const auto module_no_memory =
        fizzy_parse(wasm_no_memory.data(), wasm_no_memory.size(), nullptr);
    ASSERT_NE(module_no_memory, nullptr);

    EXPECT_FALSE(fizzy_module_has_memory(module_no_memory));

    fizzy_free_module(module_no_memory);

    /* wat2wasm
      (memory 0)
    */
    const auto wasm_memory_empty = from_hex("0061736d010000000503010000");
    const auto module_memory_empty =
        fizzy_parse(wasm_memory_empty.data(), wasm_memory_empty.size(), nullptr);
    ASSERT_NE(module_memory_empty, nullptr);

    EXPECT_TRUE(fizzy_module_has_memory(module_memory_empty));

    fizzy_free_module(module_memory_empty);

    /* wat2wasm
      (memory 1)
    */
    const auto wasm_memory = from_hex("0061736d010000000503010001");
    const auto module_memory = fizzy_parse(wasm_memory.data(), wasm_memory.size(), nullptr);
    ASSERT_NE(module_memory, nullptr);

    EXPECT_TRUE(fizzy_module_has_memory(module_memory));

    fizzy_free_module(module_memory);

    /* wat2wasm
      (memory (import "mod" "mem") 1)
    */
    const auto wasm_imported_mem = from_hex("0061736d01000000020c01036d6f64036d656d020001");
    const auto module_imported_mem =
        fizzy_parse(wasm_imported_mem.data(), wasm_imported_mem.size(), nullptr);
    ASSERT_NE(module_imported_mem, nullptr);

    EXPECT_TRUE(fizzy_module_has_memory(module_imported_mem));

    fizzy_free_module(module_imported_mem);
}

TEST(capi_module, has_start_function)
{
    /* wat2wasm
      (module)
    */
    const auto wasm_no_start = from_hex("0061736d01000000");
    const auto module_no_start = fizzy_parse(wasm_no_start.data(), wasm_no_start.size(), nullptr);
    ASSERT_NE(module_no_start, nullptr);

    EXPECT_FALSE(fizzy_module_has_start_function(module_no_start));

    fizzy_free_module(module_no_start);

    /* wat2wasm
      (func)
      (start 0)
    */
    const auto wasm_start = from_hex("0061736d01000000010401600000030201000801000a040102000b");
    const auto module_start = fizzy_parse(wasm_start.data(), wasm_start.size(), nullptr);
    ASSERT_NE(module_start, nullptr);

    EXPECT_TRUE(fizzy_module_has_start_function(module_start));

    fizzy_free_module(module_start);
}

TEST(capi_module, get_type_count)
{
    /* wat2wasm
      (module)
    */
    const auto wasm = from_hex("0061736d01000000");

    const auto* module_empty = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module_empty, nullptr);

    EXPECT_EQ(fizzy_get_type_count(module_empty), 0);
    fizzy_free_module(module_empty);

    /* wat2wasm
      (func)
    */
    const auto wasm_one_func = from_hex("0061736d01000000010401600000030201000a040102000b");

    const auto* module_one_func = fizzy_parse(wasm_one_func.data(), wasm_one_func.size(), nullptr);
    ASSERT_NE(module_one_func, nullptr);

    EXPECT_EQ(fizzy_get_type_count(module_one_func), 1);
    fizzy_free_module(module_one_func);

    /* wat2wasm
      (type (func (param i32)))
      (type (func (param i32) (result i32)))
      (type (func (result i32)))
      (func (type 0))
      (func (type 1) (return (i32.const 0)))
      (func (type 1) (return (i32.const 0)))
      (func (type 2) (return (i32.const 0)))
    */
    const auto wasm_three_types = from_hex(
        "0061736d01000000010e0360017f0060017f017f6000017f030504000101020a160402000b050041000f0b0500"
        "41000f0b050041000f0b");

    const auto* module_three_types =
        fizzy_parse(wasm_three_types.data(), wasm_three_types.size(), nullptr);
    ASSERT_NE(module_three_types, nullptr);

    EXPECT_EQ(fizzy_get_type_count(module_three_types), 3);
    fizzy_free_module(module_three_types);
}

TEST(capi_module, get_type)
{
    /* wat2wasm
      (func)
    */
    const auto wasm_one_func = from_hex("0061736d01000000010401600000030201000a040102000b");

    const auto* module_one_func = fizzy_parse(wasm_one_func.data(), wasm_one_func.size(), nullptr);
    ASSERT_NE(module_one_func, nullptr);
    ASSERT_EQ(fizzy_get_type_count(module_one_func), 1);

    const auto type_one_func = fizzy_get_type(module_one_func, 0);
    EXPECT_EQ(type_one_func.inputs_size, 0);
    EXPECT_EQ(type_one_func.output, FizzyValueTypeVoid);
    fizzy_free_module(module_one_func);

    /* wat2wasm
      (type (func))
      (type (func (param i32)))
      (type (func (param i32) (result i32)))
      (type (func (result i32)))
      (func (type 0))
      (func (type 1))
      (func (type 2) (return (i32.const 0)))
      (func (type 2) (return (i32.const 0)))
      (func (type 3) (return (i32.const 0)))
    */
    const auto wasm_three_types = from_hex(
        "0061736d0100000001110460000060017f0060017f017f6000017f03060500010202030a190502000b02000b05"
        "0041000f0b050041000f0b050041000f0b");

    const auto* module_three_types =
        fizzy_parse(wasm_three_types.data(), wasm_three_types.size(), nullptr);
    ASSERT_NE(module_three_types, nullptr);
    ASSERT_EQ(fizzy_get_type_count(module_three_types), 4);

    const auto type0 = fizzy_get_type(module_three_types, 0);
    ASSERT_EQ(type0.inputs_size, 0);
    EXPECT_EQ(type0.output, FizzyValueTypeVoid);

    const auto type1 = fizzy_get_type(module_three_types, 1);
    ASSERT_EQ(type1.inputs_size, 1);
    EXPECT_EQ(type1.inputs[0], FizzyValueTypeI32);
    EXPECT_EQ(type1.output, FizzyValueTypeVoid);

    const auto type2 = fizzy_get_type(module_three_types, 2);
    ASSERT_EQ(type2.inputs_size, 1);
    EXPECT_EQ(type2.inputs[0], FizzyValueTypeI32);
    EXPECT_EQ(type2.output, FizzyValueTypeI32);

    const auto type3 = fizzy_get_type(module_three_types, 3);
    EXPECT_EQ(type3.inputs_size, 0);
    EXPECT_EQ(type3.output, FizzyValueTypeI32);

    fizzy_free_module(module_three_types);

    /* wat2wasm
      (func (import "mod" "f") (param i64 i64))
      (func (param i32))
    */
    const auto wasm_imported_func = from_hex(
        "0061736d01000000010a0260027e7e0060017f00020901036d6f6401660000030201010a040102000b");

    const auto* module_imported_func =
        fizzy_parse(wasm_imported_func.data(), wasm_imported_func.size(), nullptr);
    ASSERT_NE(module_imported_func, nullptr);
    ASSERT_EQ(fizzy_get_type_count(module_imported_func), 2);

    const auto type_imported = fizzy_get_type(module_imported_func, 0);
    ASSERT_EQ(type_imported.inputs_size, 2);
    EXPECT_EQ(type_imported.inputs[0], FizzyValueTypeI64);
    EXPECT_EQ(type_imported.inputs[1], FizzyValueTypeI64);
    EXPECT_EQ(type_imported.output, FizzyValueTypeVoid);

    const auto type_local = fizzy_get_type(module_imported_func, 1);
    ASSERT_EQ(type_local.inputs_size, 1);
    EXPECT_EQ(type_local.inputs[0], FizzyValueTypeI32);
    EXPECT_EQ(type_local.output, FizzyValueTypeVoid);

    fizzy_free_module(module_imported_func);
}

TEST(capi_module, get_import_count)
{
    /* wat2wasm
      (module)
    */
    const auto wasm_empty = from_hex("0061736d01000000");

    const auto* module_empty = fizzy_parse(wasm_empty.data(), wasm_empty.size(), nullptr);
    ASSERT_NE(module_empty, nullptr);

    EXPECT_EQ(fizzy_get_import_count(module_empty), 0);
    fizzy_free_module(module_empty);

    /* wat2wasm
      (func (import "m" "f") (result i32))
      (global (import "m" "g") i32)
      (table (import "m" "t") 0 anyfunc)
      (memory (import "m" "m") 1)
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f021d04016d01660000016d0167037f00016d017401700000016d016d0200"
        "01");

    const auto* module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    EXPECT_EQ(fizzy_get_import_count(module), 4);
    fizzy_free_module(module);
}

TEST(capi_module, get_import_description)
{
    /* wat2wasm
      (func (import "m" "f1"))
      (func (import "m" "f2") (result i32))
      (func (import "m" "f3") (param i64))
      (func (import "m" "f4") (param f32 f64) (result i64))
      (global (import "m" "g1") i32)
      (global (import "m" "g2") (mut f64))
      (table (import "m" "t") 10 anyfunc)
      (memory (import "m" "mem") 1 4)
    */
    const auto wasm = from_hex(
        "0061736d010000000112046000006000017f60017e0060027d7c017e023f08016d0266310000016d0266320001"
        "016d0266330002016d0266340003016d026731037f00016d026732037c01016d01740170000a016d036d656d02"
        "010104");

    const auto* module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    ASSERT_EQ(fizzy_get_import_count(module), 8);

    const auto import0 = fizzy_get_import_description(module, 0);
    EXPECT_STREQ(import0.module, "m");
    EXPECT_STREQ(import0.name, "f1");
    EXPECT_EQ(import0.kind, FizzyExternalKindFunction);
    EXPECT_EQ(import0.desc.function_type.inputs_size, 0);
    EXPECT_EQ(import0.desc.function_type.output, FizzyValueTypeVoid);

    const auto import1 = fizzy_get_import_description(module, 1);
    EXPECT_STREQ(import1.module, "m");
    EXPECT_STREQ(import1.name, "f2");
    EXPECT_EQ(import1.kind, FizzyExternalKindFunction);
    EXPECT_EQ(import1.desc.function_type.inputs_size, 0);
    EXPECT_EQ(import1.desc.function_type.output, FizzyValueTypeI32);

    const auto import2 = fizzy_get_import_description(module, 2);
    EXPECT_STREQ(import2.module, "m");
    EXPECT_STREQ(import2.name, "f3");
    EXPECT_EQ(import2.kind, FizzyExternalKindFunction);
    ASSERT_EQ(import2.desc.function_type.inputs_size, 1);
    EXPECT_EQ(import2.desc.function_type.inputs[0], FizzyValueTypeI64);
    EXPECT_EQ(import2.desc.function_type.output, FizzyValueTypeVoid);

    const auto import3 = fizzy_get_import_description(module, 3);
    EXPECT_STREQ(import3.module, "m");
    EXPECT_STREQ(import3.name, "f4");
    EXPECT_EQ(import3.kind, FizzyExternalKindFunction);
    ASSERT_EQ(import3.desc.function_type.inputs_size, 2);
    EXPECT_EQ(import3.desc.function_type.inputs[0], FizzyValueTypeF32);
    EXPECT_EQ(import3.desc.function_type.inputs[1], FizzyValueTypeF64);
    EXPECT_EQ(import3.desc.function_type.output, FizzyValueTypeI64);

    const auto import4 = fizzy_get_import_description(module, 4);
    EXPECT_STREQ(import4.module, "m");
    EXPECT_STREQ(import4.name, "g1");
    EXPECT_EQ(import4.kind, FizzyExternalKindGlobal);
    EXPECT_EQ(import4.desc.global_type.value_type, FizzyValueTypeI32);
    EXPECT_FALSE(import4.desc.global_type.is_mutable);

    const auto import5 = fizzy_get_import_description(module, 5);
    EXPECT_STREQ(import5.module, "m");
    EXPECT_STREQ(import5.name, "g2");
    EXPECT_EQ(import5.kind, FizzyExternalKindGlobal);
    EXPECT_EQ(import5.desc.global_type.value_type, FizzyValueTypeF64);
    EXPECT_TRUE(import5.desc.global_type.is_mutable);

    const auto import6 = fizzy_get_import_description(module, 6);
    EXPECT_STREQ(import6.module, "m");
    EXPECT_STREQ(import6.name, "t");
    EXPECT_EQ(import6.kind, FizzyExternalKindTable);
    EXPECT_EQ(import6.desc.table_limits.min, 10);
    EXPECT_FALSE(import6.desc.table_limits.has_max);

    const auto import7 = fizzy_get_import_description(module, 7);
    EXPECT_STREQ(import7.module, "m");
    EXPECT_STREQ(import7.name, "mem");
    EXPECT_EQ(import7.kind, FizzyExternalKindMemory);
    EXPECT_EQ(import7.desc.memory_limits.min, 1);
    EXPECT_TRUE(import7.desc.memory_limits.has_max);
    EXPECT_EQ(import7.desc.memory_limits.max, 4);

    fizzy_free_module(module);
}

TEST(capi_module, import_name_after_instantiate)
{
    /* wat2wasm
      (func (import "m" "f1") (result i32))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020801016d0266310000");

    const auto* module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    ASSERT_EQ(fizzy_get_import_count(module), 1);

    const auto import0 = fizzy_get_import_description(module, 0);
    EXPECT_STREQ(import0.module, "m");
    EXPECT_STREQ(import0.name, "f1");

    FizzyExternalFunction host_funcs[] = {{{FizzyValueTypeI32, nullptr, 0}, NullFn, nullptr}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    EXPECT_STREQ(import0.module, "m");
    EXPECT_STREQ(import0.name, "f1");

    fizzy_free_instance(instance);
}

TEST(capi_module, get_global_count)
{
    /* wat2wasm
      (module)
    */
    const auto wasm = from_hex("0061736d01000000");

    const auto* module_empty = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module_empty, nullptr);

    EXPECT_EQ(fizzy_get_global_count(module_empty), 0);
    fizzy_free_module(module_empty);

    /* wat2wasm
      (global i32 (i32.const 0))
    */
    const auto wasm_one_global = from_hex("0061736d010000000606017f0041000b");

    const auto* module_one_global =
        fizzy_parse(wasm_one_global.data(), wasm_one_global.size(), nullptr);
    ASSERT_NE(module_one_global, nullptr);

    EXPECT_EQ(fizzy_get_global_count(module_one_global), 1);
    fizzy_free_module(module_one_global);

    /* wat2wasm
      (global (import "mod" "g") i32)
      (global i32 (i32.const 0))
    */
    const auto wasm_imported_global =
        from_hex("0061736d01000000020a01036d6f640167037f000606017f0041000b");

    const auto* module_imported_global =
        fizzy_parse(wasm_imported_global.data(), wasm_imported_global.size(), nullptr);
    ASSERT_NE(module_imported_global, nullptr);

    EXPECT_EQ(fizzy_get_global_count(module_imported_global), 2);
    fizzy_free_module(module_imported_global);
}

TEST(capi_module, get_global_type)
{
    /* wat2wasm
      (global i32 (i32.const 0))
      (global (mut i64) (i64.const 0))
      (global f32 (f32.const 0))
      (global (mut f64) (f64.const 0))
    */
    const auto wasm = from_hex(
        "0061736d01000000061f047f0041000b7e0142000b7d0043000000000b7c014400000000000000000b");

    const auto* module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    ASSERT_EQ(fizzy_get_global_count(module), 4);

    EXPECT_EQ(fizzy_get_global_type(module, 0).value_type, FizzyValueTypeI32);
    EXPECT_EQ(fizzy_get_global_type(module, 0).is_mutable, false);
    EXPECT_EQ(fizzy_get_global_type(module, 1).value_type, FizzyValueTypeI64);
    EXPECT_EQ(fizzy_get_global_type(module, 1).is_mutable, true);
    EXPECT_EQ(fizzy_get_global_type(module, 2).value_type, FizzyValueTypeF32);
    EXPECT_EQ(fizzy_get_global_type(module, 2).is_mutable, false);
    EXPECT_EQ(fizzy_get_global_type(module, 3).value_type, FizzyValueTypeF64);
    EXPECT_EQ(fizzy_get_global_type(module, 3).is_mutable, true);

    fizzy_free_module(module);

    /* wat2wasm
      (global (import "mod" "g1") i32)
      (global (import "mod" "g2") (mut i64))
      (global (import "mod" "g3") f32)
      (global (import "mod" "g4") (mut f64))
    */
    const auto wasm_imports = from_hex(
        "0061736d01000000022904036d6f64026731037f00036d6f64026732037e01036d6f64026733037d00036d6f64"
        "026734037c01");

    const auto* module_imports = fizzy_parse(wasm_imports.data(), wasm_imports.size(), nullptr);
    ASSERT_NE(module_imports, nullptr);
    ASSERT_EQ(fizzy_get_global_count(module_imports), 4);

    EXPECT_EQ(fizzy_get_global_type(module_imports, 0).value_type, FizzyValueTypeI32);
    EXPECT_EQ(fizzy_get_global_type(module_imports, 0).is_mutable, false);
    EXPECT_EQ(fizzy_get_global_type(module_imports, 1).value_type, FizzyValueTypeI64);
    EXPECT_EQ(fizzy_get_global_type(module_imports, 1).is_mutable, true);
    EXPECT_EQ(fizzy_get_global_type(module_imports, 2).value_type, FizzyValueTypeF32);
    EXPECT_EQ(fizzy_get_global_type(module_imports, 2).is_mutable, false);
    EXPECT_EQ(fizzy_get_global_type(module_imports, 3).value_type, FizzyValueTypeF64);
    EXPECT_EQ(fizzy_get_global_type(module_imports, 3).is_mutable, true);

    fizzy_free_module(module_imports);
}
