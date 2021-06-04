// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy::test;

TEST(capi_exports, get_export_count)
{
    /* wat2wasm
      (module)
    */
    const auto wasm_empty = from_hex("0061736d01000000");

    const auto* module_empty = fizzy_parse(wasm_empty.data(), wasm_empty.size(), nullptr);
    ASSERT_NE(module_empty, nullptr);

    EXPECT_EQ(fizzy_get_export_count(module_empty), 0);
    fizzy_free_module(module_empty);

    /* wat2wasm
      (func (export "f"))
      (global (export "g") i32 (i32.const 0))
      (table (export "t") 0 anyfunc)
      (memory (export "m") 1)
    */
    const auto wasm = from_hex(
        "0061736d010000000104016000000302010004040170000005030100010606017f0041000b0711040166000001"
        "67030001740100016d02000a040102000b");

    const auto* module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    EXPECT_EQ(fizzy_get_export_count(module), 4);
    fizzy_free_module(module);
}

TEST(capi_exports, get_export_description)
{
    /* wat2wasm
      (func) ;; to make export have non-zero index
      (func (export "fn"))
      (table (export "tab") 10 anyfunc)
      (memory (export "mem") 1 4)
      (global i32 (i32.const 0))
      (global i32 (i32.const 0))
      (global (export "glob") i32 (i32.const 0))
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030302000004040170000a0504010101040610037f0041000b7f0041000b7f"
        "0041000b07190402666e0001037461620100036d656d020004676c6f6203020a070202000b02000b");

    const auto* module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    ASSERT_EQ(fizzy_get_export_count(module), 4);

    const auto export0 = fizzy_get_export_description(module, 0);
    EXPECT_STREQ(export0.name, "fn");
    EXPECT_EQ(export0.kind, FizzyExternalKindFunction);
    EXPECT_EQ(export0.index, 1);

    const auto export1 = fizzy_get_export_description(module, 1);
    EXPECT_STREQ(export1.name, "tab");
    EXPECT_EQ(export1.kind, FizzyExternalKindTable);
    EXPECT_EQ(export1.index, 0);

    const auto export2 = fizzy_get_export_description(module, 2);
    EXPECT_STREQ(export2.name, "mem");
    EXPECT_EQ(export2.kind, FizzyExternalKindMemory);
    EXPECT_EQ(export2.index, 0);

    const auto export3 = fizzy_get_export_description(module, 3);
    EXPECT_STREQ(export3.name, "glob");
    EXPECT_EQ(export3.kind, FizzyExternalKindGlobal);
    EXPECT_EQ(export3.index, 2);

    fizzy_free_module(module);
}

TEST(capi_exports, export_name_after_instantiate)
{
    /* wat2wasm
      (func (export "fn"))
    */
    const auto wasm = from_hex("0061736d010000000104016000000302010007060102666e00000a040102000b");

    const auto* module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    ASSERT_EQ(fizzy_get_export_count(module), 1);

    const auto export0 = fizzy_get_export_description(module, 0);
    EXPECT_STREQ(export0.name, "fn");

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    EXPECT_STREQ(export0.name, "fn");

    fizzy_free_instance(instance);
}

TEST(capi_exports, find_exported_function_index)
{
    /* wat2wasm
    (module
      (func $f (export "foo") (result i32) (i32.const 42))
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f030201000404017000000504010101020606017f0041000b07180403666f"
        "6f00000267310300037461620100036d656d02000a06010400412a0b");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    uint32_t func_idx;
    EXPECT_TRUE(fizzy_find_exported_function_index(module, "foo", &func_idx));
    EXPECT_EQ(func_idx, 0);

    EXPECT_FALSE(fizzy_find_exported_function_index(module, "bar", &func_idx));
    EXPECT_FALSE(fizzy_find_exported_function_index(module, "g1", &func_idx));
    EXPECT_FALSE(fizzy_find_exported_function_index(module, "tab", &func_idx));
    EXPECT_FALSE(fizzy_find_exported_function_index(module, "mem", &func_idx));

    fizzy_free_module(module);
}

TEST(capi_exports, find_exported_function)
{
    /* wat2wasm
    (module
      (func $f (export "foo") (result i32) (i32.const 42))
      (global (export "g1") i32 (i32.const 42))
      (table (export "tab") 10 30 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f0302010004050170010a1e0504010101020606017f00412a0b0718040366"
        "6f6f00000267310300037461620100036d656d02000a06010400412a0b");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    FizzyExternalFunction function;
    ASSERT_TRUE(fizzy_find_exported_function(instance, "foo", &function));
    EXPECT_EQ(function.type.inputs_size, 0);
    EXPECT_EQ(function.type.output, FizzyValueTypeI32);
    EXPECT_NE(function.context, nullptr);
    ASSERT_NE(function.function, nullptr);

    EXPECT_THAT(function.function(function.context, instance, nullptr, nullptr), CResult(42_u32));

    fizzy_free_exported_function(&function);

    EXPECT_FALSE(fizzy_find_exported_function(instance, "foo2", &function));
    EXPECT_FALSE(fizzy_find_exported_function(instance, "g1", &function));
    EXPECT_FALSE(fizzy_find_exported_function(instance, "tab", &function));
    EXPECT_FALSE(fizzy_find_exported_function(instance, "mem", &function));

    fizzy_free_instance(instance);
}

TEST(capi_exports, find_exported_table)
{
    /* wat2wasm
    (module
      (func $f (export "foo") (result i32) (i32.const 42))
      (global (export "g1") i32 (i32.const 42))
      (table (export "tab") 10 30 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f0302010004050170010a1e0504010101020606017f00412a0b0718040366"
        "6f6f00000267310300037461620100036d656d02000a06010400412a0b");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    FizzyExternalTable table;
    ASSERT_TRUE(fizzy_find_exported_table(instance, "tab", &table));
    EXPECT_NE(table.table, nullptr);
    EXPECT_EQ(table.limits.min, 10);
    EXPECT_TRUE(table.limits.has_max);
    EXPECT_EQ(table.limits.max, 30);

    EXPECT_FALSE(fizzy_find_exported_table(instance, "tab2", &table));
    EXPECT_FALSE(fizzy_find_exported_table(instance, "foo", &table));
    EXPECT_FALSE(fizzy_find_exported_table(instance, "g1", &table));
    EXPECT_FALSE(fizzy_find_exported_table(instance, "mem", &table));

    fizzy_free_instance(instance);
}

TEST(capi_exports, find_exported_table_no_max)
{
    /* wat2wasm
    (module
      (table (export "tab") 1 anyfunc)
    )
    */
    const auto wasm = from_hex("0061736d01000000040401700001070701037461620100");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    FizzyExternalTable table;
    ASSERT_TRUE(fizzy_find_exported_table(instance, "tab", &table));
    EXPECT_NE(table.table, nullptr);
    EXPECT_EQ(table.limits.min, 1);
    EXPECT_FALSE(table.limits.has_max);

    fizzy_free_instance(instance);
}

TEST(capi_exports, find_exported_memory)
{
    /* wat2wasm
    (module
      (func (export "foo") (result i32) (i32.const 42))
      (global (export "g1") i32 (i32.const 42))
      (table (export "tab") 10 30 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f0302010004050170010a1e0504010101020606017f00412a0b0718040366"
        "6f6f00000267310300037461620100036d656d02000a06010400412a0b");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    FizzyExternalMemory memory;
    ASSERT_TRUE(fizzy_find_exported_memory(instance, "mem", &memory));
    EXPECT_NE(memory.memory, nullptr);
    EXPECT_EQ(memory.limits.min, 1);
    EXPECT_TRUE(memory.limits.has_max);
    EXPECT_EQ(memory.limits.max, 2);

    EXPECT_FALSE(fizzy_find_exported_memory(instance, "mem2", &memory));
    EXPECT_FALSE(fizzy_find_exported_memory(instance, "foo", &memory));
    EXPECT_FALSE(fizzy_find_exported_memory(instance, "g1", &memory));
    EXPECT_FALSE(fizzy_find_exported_memory(instance, "tab", &memory));

    fizzy_free_instance(instance);
}

TEST(capi_exports, find_exported_memory_no_max)
{
    /* wat2wasm
    (module
      (memory (export "mem") 1)
    )
    */
    const auto wasm = from_hex("0061736d010000000503010001070701036d656d0200");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    FizzyExternalMemory memory;
    ASSERT_TRUE(fizzy_find_exported_memory(instance, "mem", &memory));
    EXPECT_NE(memory.memory, nullptr);
    EXPECT_EQ(memory.limits.min, 1);
    EXPECT_FALSE(memory.limits.has_max);

    fizzy_free_instance(instance);
}

TEST(capi_exports, find_exported_global)
{
    /* wat2wasm
    (module
      (func $f (export "foo") (result i32) (i32.const 42))
      (global (export "g1") i32 (i32.const 42))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f030201000404017000000504010101020606017f00412a0b07180403666f"
        "6f00000267310300037461620100036d656d02000a06010400412a0b");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    FizzyExternalGlobal global;
    ASSERT_TRUE(fizzy_find_exported_global(instance, "g1", &global));
    EXPECT_EQ(global.type.value_type, FizzyValueTypeI32);
    EXPECT_FALSE(global.type.is_mutable);
    EXPECT_EQ(global.value->i32, 42);

    EXPECT_FALSE(fizzy_find_exported_global(instance, "g2", &global));
    EXPECT_FALSE(fizzy_find_exported_global(instance, "foo", &global));
    EXPECT_FALSE(fizzy_find_exported_global(instance, "tab", &global));
    EXPECT_FALSE(fizzy_find_exported_global(instance, "mem", &global));

    fizzy_free_instance(instance);
}
