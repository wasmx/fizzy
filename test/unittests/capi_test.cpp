// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy::test;

static_assert(FizzyMemoryPagesLimitDefault == fizzy::DefaultMemoryPagesLimit);

/// Represents an invalid/mocked pointer to a host function for tests without execution.
static constexpr FizzyExternalFn NullFn = nullptr;

TEST(capi, validate)
{
    uint8_t wasm_prefix[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};

    // Success omitting FizzyError argument.
    EXPECT_TRUE(fizzy_validate(wasm_prefix, sizeof(wasm_prefix), nullptr));

    // Success with FizzyError argument.
    FizzyError success;
    EXPECT_TRUE(fizzy_validate(wasm_prefix, sizeof(wasm_prefix), &success));
    EXPECT_EQ(success.code, FizzySuccess);
    EXPECT_STREQ(success.message, "");

    wasm_prefix[7] = 1;
    // Parsing error omitting FizzyError argument.
    EXPECT_FALSE(fizzy_validate(wasm_prefix, sizeof(wasm_prefix), nullptr));

    // Parsing error with FizzyError argument.
    FizzyError parsing_error;
    EXPECT_FALSE(fizzy_validate(wasm_prefix, sizeof(wasm_prefix), &parsing_error));
    EXPECT_EQ(parsing_error.code, FizzyErrorMalformedModule);
    EXPECT_STREQ(parsing_error.message, "invalid wasm module prefix");

    /* wat2wasm --no-check
      (func (i32.const 0))
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0601040041000b");
    // Validation error omitting FizzyError argument.
    EXPECT_FALSE(fizzy_validate(wasm.data(), wasm.size(), nullptr));

    // Validation error with FizzyError argument.
    FizzyError validation_error;
    EXPECT_FALSE(fizzy_validate(wasm.data(), wasm.size(), &validation_error));
    EXPECT_EQ(validation_error.code, FizzyErrorInvalidModule);
    EXPECT_STREQ(validation_error.message, "too many results");
}

TEST(capi, error_reuse)
{
    uint8_t wasm_prefix[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};

    FizzyError error;
    EXPECT_TRUE(fizzy_validate(wasm_prefix, sizeof(wasm_prefix), &error));
    EXPECT_EQ(error.code, FizzySuccess);
    EXPECT_STREQ(error.message, "");

    wasm_prefix[7] = 1;
    EXPECT_FALSE(fizzy_validate(wasm_prefix, sizeof(wasm_prefix), &error));
    EXPECT_EQ(error.code, FizzyErrorMalformedModule);
    EXPECT_STREQ(error.message, "invalid wasm module prefix");

    /* wat2wasm --no-check
      (func (i32.const 0))
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0601040041000b");
    EXPECT_FALSE(fizzy_validate(wasm.data(), wasm.size(), &error));
    EXPECT_EQ(error.code, FizzyErrorInvalidModule);
    EXPECT_STREQ(error.message, "too many results");
}

TEST(capi, truncated_error_message)
{
    // "duplicate export name " error message prefix is 22 characters long

    // Export name 233 characters long, will be not truncated.
    // clang-format off
    /* wat2wasm --no-check
      (func (export "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. D"))
      (func (export "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. D"))
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030302000007db0302e9014c6f72656d20697073756d20646f6c6f72207369"
        "7420616d65742c20636f6e73656374657475722061646970697363696e6720656c69742c2073656420646f2065"
        "6975736d6f642074656d706f7220696e6369646964756e74207574206c61626f726520657420646f6c6f726520"
        "6d61676e6120616c697175612e20557420656e696d206164206d696e696d2076656e69616d2c2071756973206e"
        "6f737472756420657865726369746174696f6e20756c6c616d636f206c61626f726973206e6973692075742061"
        "6c697175697020657820656120636f6d6d6f646f20636f6e7365717561742e20440000e9014c6f72656d206970"
        "73756d20646f6c6f722073697420616d65742c20636f6e73656374657475722061646970697363696e6720656c"
        "69742c2073656420646f20656975736d6f642074656d706f7220696e6369646964756e74207574206c61626f72"
        "6520657420646f6c6f7265206d61676e6120616c697175612e20557420656e696d206164206d696e696d207665"
        "6e69616d2c2071756973206e6f737472756420657865726369746174696f6e20756c6c616d636f206c61626f72"
        "6973206e69736920757420616c697175697020657820656120636f6d6d6f646f20636f6e7365717561742e2044"
        "00010a070202000b02000b");
    // clang-format on

    FizzyError error;
    EXPECT_FALSE(fizzy_validate(wasm.data(), wasm.size(), &error));
    EXPECT_EQ(error.code, FizzyErrorInvalidModule);
    EXPECT_EQ(strlen(error.message), 255);
    EXPECT_STREQ(error.message,
        "duplicate export name Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis "
        "nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. D");

    // Export name 234 characters long, will be truncated.
    // clang-format off
    /* wat2wasm --no-check
      (func (export "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Du"))
      (func (export "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Du"))
    */
    const auto wasm_trunc = from_hex(
        "0061736d01000000010401600000030302000007dd0302ea014c6f72656d20697073756d20646f6c6f72207369"
        "7420616d65742c20636f6e73656374657475722061646970697363696e6720656c69742c2073656420646f2065"
        "6975736d6f642074656d706f7220696e6369646964756e74207574206c61626f726520657420646f6c6f726520"
        "6d61676e6120616c697175612e20557420656e696d206164206d696e696d2076656e69616d2c2071756973206e"
        "6f737472756420657865726369746174696f6e20756c6c616d636f206c61626f726973206e6973692075742061"
        "6c697175697020657820656120636f6d6d6f646f20636f6e7365717561742e2044750000ea014c6f72656d2069"
        "7073756d20646f6c6f722073697420616d65742c20636f6e73656374657475722061646970697363696e672065"
        "6c69742c2073656420646f20656975736d6f642074656d706f7220696e6369646964756e74207574206c61626f"
        "726520657420646f6c6f7265206d61676e6120616c697175612e20557420656e696d206164206d696e696d2076"
        "656e69616d2c2071756973206e6f737472756420657865726369746174696f6e20756c6c616d636f206c61626f"
        "726973206e69736920757420616c697175697020657820656120636f6d6d6f646f20636f6e7365717561742e20"
        "447500010a070202000b02000b");
    // clang-format on

    EXPECT_FALSE(fizzy_validate(wasm_trunc.data(), wasm_trunc.size(), &error));
    EXPECT_EQ(error.code, FizzyErrorInvalidModule);
    EXPECT_EQ(strlen(error.message), 255);
    EXPECT_STREQ(error.message,
        "duplicate export name Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do "
        "eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis "
        "nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat...");
}

TEST(capi, parse)
{
    uint8_t wasm_prefix[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    // Success omitting FizzyError argument.
    auto module = fizzy_parse(wasm_prefix, sizeof(wasm_prefix), nullptr);
    EXPECT_NE(module, nullptr);
    fizzy_free_module(module);

    // Success with FizzyError argument.
    FizzyError success;
    module = fizzy_parse(wasm_prefix, sizeof(wasm_prefix), &success);
    EXPECT_NE(module, nullptr);
    EXPECT_EQ(success.code, FizzySuccess);
    EXPECT_STREQ(success.message, "");
    fizzy_free_module(module);

    wasm_prefix[7] = 1;
    // Parsing error omitting FizzyError argument.
    EXPECT_EQ(fizzy_parse(wasm_prefix, sizeof(wasm_prefix), nullptr), nullptr);

    // Parsing error with FizzyError argument.
    FizzyError parsing_error;
    EXPECT_FALSE(fizzy_parse(wasm_prefix, sizeof(wasm_prefix), &parsing_error));
    EXPECT_EQ(parsing_error.code, FizzyErrorMalformedModule);
    EXPECT_STREQ(parsing_error.message, "invalid wasm module prefix");

    /* wat2wasm --no-check
      (func (i32.const 0))
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0601040041000b");
    // Validation error omitting FizzyError argument.
    EXPECT_FALSE(fizzy_parse(wasm.data(), wasm.size(), nullptr));

    // Validation error with FizzyError argument.
    FizzyError validation_error;
    EXPECT_FALSE(fizzy_parse(wasm.data(), wasm.size(), &validation_error));
    EXPECT_EQ(validation_error.code, FizzyErrorInvalidModule);
    EXPECT_STREQ(validation_error.message, "too many results");
}

TEST(capi, free_module_null)
{
    fizzy_free_module(nullptr);
}

TEST(capi, clone_module)
{
    /* wat2wasm
      (func (param i32 i32) (result i32) (i32.const 0))
    */
    const auto wasm = from_hex("0061736d0100000001070160027f7f017f030201000a0601040041000b");
    const auto* module1 = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    EXPECT_NE(module1, nullptr);

    const auto* module2 = fizzy_clone_module(module1);
    EXPECT_NE(module2, nullptr);
    EXPECT_NE(module1, module2);

    const auto type2 = fizzy_get_function_type(module2, 0);
    EXPECT_EQ(type2.output, FizzyValueTypeI32);
    ASSERT_EQ(type2.inputs_size, 2);
    EXPECT_EQ(type2.inputs[0], FizzyValueTypeI32);
    EXPECT_EQ(type2.inputs[1], FizzyValueTypeI32);

    fizzy_free_module(module2);
    fizzy_free_module(module1);
}

TEST(capi, get_function_type)
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

TEST(capi, has_table)
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

TEST(capi, has_memory)
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

TEST(capi, get_export_count)
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

TEST(capi, get_export_description)
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

TEST(capi, export_name_after_instantiate)
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

TEST(capi, find_exported_function_index)
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

TEST(capi, find_exported_function)
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
    EXPECT_EQ(function.context, nullptr);
    ASSERT_EQ(function.function, nullptr);
    EXPECT_EQ(function.instance, instance);
    EXPECT_EQ(function.function_index, 0);


    // EXPECT_THAT(function.function(function.context, instance, nullptr, nullptr),
    // CResult(42_u32));

    EXPECT_FALSE(fizzy_find_exported_function(instance, "foo2", &function));
    EXPECT_FALSE(fizzy_find_exported_function(instance, "g1", &function));
    EXPECT_FALSE(fizzy_find_exported_function(instance, "tab", &function));
    EXPECT_FALSE(fizzy_find_exported_function(instance, "mem", &function));

    fizzy_free_instance(instance);
}

TEST(capi, find_exported_table)
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

TEST(capi, find_exported_table_no_max)
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

TEST(capi, find_exported_memory)
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

TEST(capi, find_exported_memory_no_max)
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

TEST(capi, find_exported_global)
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

TEST(capi, has_start_function)
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

TEST(capi, instantiate)
{
    uint8_t wasm_prefix[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    const auto* module = fizzy_parse(wasm_prefix, sizeof(wasm_prefix), nullptr);
    ASSERT_NE(module, nullptr);

    // Success omitting FizzyError argument.
    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    fizzy_free_instance(instance);

    module = fizzy_parse(wasm_prefix, sizeof(wasm_prefix), nullptr);
    ASSERT_NE(module, nullptr);

    // Success with FizzyError argument.
    FizzyError success;
    instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, &success);
    EXPECT_NE(instance, nullptr);
    EXPECT_EQ(success.code, FizzySuccess);
    EXPECT_STREQ(success.message, "");
    fizzy_free_instance(instance);
}

TEST(capi, instantiate_imported_function)
{
    /* wat2wasm
      (func (import "mod1" "foo1") (result i32))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020d01046d6f643104666f6f310000");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    // Error omitting FizzyError argument.
    EXPECT_EQ(fizzy_instantiate(module, nullptr, 0, nullptr, nullptr, nullptr, 0,
                  FizzyMemoryPagesLimitDefault, nullptr),
        nullptr);

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    // Error with FizzyError argument.
    FizzyError error;
    EXPECT_EQ(fizzy_instantiate(module, nullptr, 0, nullptr, nullptr, nullptr, 0,
                  FizzyMemoryPagesLimitDefault, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "module requires 1 imported functions, 0 provided");

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyExternalFunction host_funcs[] = {
        {{FizzyValueTypeI32, nullptr, 0}, nullptr, 0, NullFn, nullptr}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    fizzy_free_instance(instance);
}

TEST(capi, instantiate_imported_globals)
{
    /* wat2wasm
      (global (import "mod1" "g1") (mut i32))
      (global (import "mod1" "g2") i64)
      (global (import "mod1" "g3") f32)
      (global (import "mod1" "g4") (mut f64))
      (func (result i32) (global.get 0))
      (func (result i64) (global.get 1))
      (func (result f32) (global.get 2))
      (func (result f64) (global.get 3))
    */
    const auto wasm = from_hex(
        "0061736d010000000111046000017f6000017e6000017d6000017c022d04046d6f6431026731037f01046d6f64"
        "31026732037e00046d6f6431026733037d00046d6f6431026734037c01030504000102030a1504040023000b04"
        "0023010b040023020b040023030b");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyValue g1{42};
    FizzyValue g2{43};
    FizzyValue g3;
    g3.f32 = 44.4f;
    FizzyValue g4;
    g4.f64 = 45.5;
    FizzyExternalGlobal globals[] = {{&g1, {FizzyValueTypeI32, true}},
        {&g2, {FizzyValueTypeI64, false}}, {&g3, {FizzyValueTypeF32, false}},
        {&g4, {FizzyValueTypeF64, true}}};

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, globals, 4, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr), CResult(43_u64));
    EXPECT_THAT(fizzy_execute(instance, 2, nullptr), CResult(44.4f));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr), CResult(45.5));

    fizzy_free_instance(instance);

    // No globals provided.
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    FizzyError error;
    EXPECT_EQ(fizzy_instantiate(module, nullptr, 0, nullptr, nullptr, nullptr, 0,
                  FizzyMemoryPagesLimitDefault, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "module requires 4 imported globals, 0 provided");

    // Not enough globals provided.
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(fizzy_instantiate(module, nullptr, 0, nullptr, nullptr, globals, 3,
                  FizzyMemoryPagesLimitDefault, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "module requires 4 imported globals, 3 provided");

    // Incorrect order or globals.
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyExternalGlobal globals_incorrect_order[] = {{&g1, {FizzyValueTypeI32, true}},
        {&g2, {FizzyValueTypeI64, false}}, {&g4, {FizzyValueTypeF64, true}},
        {&g3, {FizzyValueTypeF32, false}}};

    EXPECT_EQ(fizzy_instantiate(module, nullptr, 0, nullptr, nullptr, globals_incorrect_order, 4,
                  FizzyMemoryPagesLimitDefault, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "global 2 value type doesn't match module's global type");

    // Global type mismatch.
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyExternalGlobal globals_type_mismatch[] = {{&g1, {FizzyValueTypeI64, true}},
        {&g2, {FizzyValueTypeI64, false}}, {&g3, {FizzyValueTypeF32, false}},
        {&g4, {FizzyValueTypeF64, true}}};

    EXPECT_EQ(fizzy_instantiate(module, nullptr, 0, nullptr, nullptr, globals_type_mismatch, 4,
                  FizzyMemoryPagesLimitDefault, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "global 0 value type doesn't match module's global type");
}

TEST(capi, instantiate_twice)
{
    uint8_t wasm_prefix[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    const auto* module1 = fizzy_parse(wasm_prefix, sizeof(wasm_prefix), nullptr);
    ASSERT_NE(module1, nullptr);

    auto* instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance1, nullptr);

    const auto* module2 = fizzy_clone_module(module1);
    ASSERT_NE(module2, nullptr);

    auto* instance2 = fizzy_instantiate(
        module2, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance2, nullptr);
    EXPECT_NE(instance1, instance2);

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi, instantiate_custom_hard_memory_limit)
{
    /* wat2wasm
      (memory 2)
    */
    const auto wasm = from_hex("0061736d010000000503010002");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    uint32_t memory_pages_limit = 2;
    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, memory_pages_limit, nullptr);
    EXPECT_NE(instance, nullptr);
    fizzy_free_instance(instance);

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    memory_pages_limit = 1;
    FizzyError error;
    EXPECT_EQ(fizzy_instantiate(
                  module, nullptr, 0, nullptr, nullptr, nullptr, 0, memory_pages_limit, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "cannot exceed hard memory limit of 65536 bytes");

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    memory_pages_limit = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(fizzy_instantiate(
                  module, nullptr, 0, nullptr, nullptr, nullptr, 0, memory_pages_limit, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "hard memory limit cannot exceed 4294967296 bytes");
}

TEST(capi, resolve_instantiate_no_imports)
{
    /* wat2wasm
      (module)
    */
    const auto wasm = from_hex("0061736d01000000");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    // Success omitting FizzyError argument.
    auto instance = fizzy_resolve_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    fizzy_free_instance(instance);

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    // Success with FizzyError argument.
    FizzyError success;
    instance = fizzy_resolve_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, &success);
    EXPECT_NE(instance, nullptr);
    EXPECT_EQ(success.code, FizzySuccess);
    EXPECT_STREQ(success.message, "");

    fizzy_free_instance(instance);

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    // providing unnecessary import
    FizzyImportedFunction host_funcs[] = {
        {"mod", "foo", {{FizzyValueTypeVoid, nullptr, 0}, nullptr, 0, NullFn, nullptr}}};

    instance = fizzy_resolve_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    fizzy_free_instance(instance);
}

TEST(capi, resolve_instantiate_functions)
{
    /* wat2wasm
      (func (import "mod1" "foo1") (param i32) (result i32))
      (func (import "mod1" "foo2") (param i32) (result i64))
      (func (import "mod2" "foo1") (param i32) (result f32))
      (func (import "mod2" "foo2") (param i32) (result f64))
      (global (import "mod1" "g1") i32) ;; just to test combination with other import types
    */
    const auto wasm = from_hex(
        "0061736d0100000001150460017f017f60017f017e60017f017d60017f017c023c05046d6f643104666f6f3100"
        "00046d6f643104666f6f320001046d6f643204666f6f310002046d6f643204666f6f320003046d6f6431026731"
        "037f00");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyValue mod1g1value{42};
    FizzyImportedGlobal mod1g1 = {"mod1", "g1", {&mod1g1value, {FizzyValueTypeI32, false}}};

    // no functions provided
    // Error omitting FizzyError argument.
    EXPECT_EQ(fizzy_resolve_instantiate(module, nullptr, 0, nullptr, nullptr, &mod1g1, 1,
                  FizzyMemoryPagesLimitDefault, nullptr),
        nullptr);

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    // Error with FizzyError argument.
    FizzyError error;
    EXPECT_EQ(fizzy_resolve_instantiate(module, nullptr, 0, nullptr, nullptr, &mod1g1, 1,
                  FizzyMemoryPagesLimitDefault, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "imported function mod1.foo1 is required");

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyExternalFn host_fn = [](void* context, FizzyInstance*, const FizzyValue*,
                                  FizzyExecutionContext*) noexcept {
        return FizzyExecutionResult{false, true, *static_cast<FizzyValue*>(context)};
    };

    const FizzyValueType input_type = FizzyValueTypeI32;
    FizzyValue result_int32{42};
    FizzyExternalFunction mod1foo1 = {
        {FizzyValueTypeI32, &input_type, 1}, nullptr, 0, host_fn, &result_int32};
    FizzyValue result_int64{43};
    FizzyExternalFunction mod1foo2 = {
        {FizzyValueTypeI64, &input_type, 1}, nullptr, 0, host_fn, &result_int64};
    FizzyValue result_f32;
    result_f32.f32 = 44.44f;
    FizzyExternalFunction mod2foo1 = {
        {FizzyValueTypeF32, &input_type, 1}, nullptr, 0, host_fn, &result_f32};
    FizzyValue result_f64;
    result_f64.f64 = 45.45;
    FizzyExternalFunction mod2foo2 = {
        {FizzyValueTypeF64, &input_type, 1}, nullptr, 0, host_fn, &result_f64};

    FizzyImportedFunction host_funcs[] = {{"mod1", "foo1", mod1foo1}, {"mod1", "foo2", mod1foo2},
        {"mod2", "foo1", mod2foo1}, {"mod2", "foo2", mod2foo2}};

    auto instance = fizzy_resolve_instantiate(
        module, host_funcs, 4, nullptr, nullptr, &mod1g1, 1, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    FizzyValue arg;
    EXPECT_THAT(fizzy_execute(instance, 0, &arg), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 1, &arg), CResult(43_u64));
    EXPECT_THAT(fizzy_execute(instance, 2, &arg), CResult(44.44f));
    EXPECT_THAT(fizzy_execute(instance, 3, &arg), CResult(45.45));

    fizzy_free_instance(instance);

    // reordered functions
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    FizzyImportedFunction host_funcs_reordered[] = {{"mod1", "foo2", mod1foo2},
        {"mod2", "foo1", mod2foo1}, {"mod2", "foo2", mod2foo2}, {"mod1", "foo1", mod1foo1}};
    instance = fizzy_resolve_instantiate(module, host_funcs_reordered, 4, nullptr, nullptr, &mod1g1,
        1, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);
    fizzy_free_instance(instance);

    // extra functions
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    FizzyImportedFunction host_funcs_extra[] = {{"mod1", "foo1", mod1foo1},
        {"mod1", "foo2", mod1foo2}, {"mod2", "foo1", mod2foo1}, {"mod2", "foo2", mod2foo2},
        {"mod3", "foo1", mod1foo1}};
    instance = fizzy_resolve_instantiate(module, host_funcs_extra, 4, nullptr, nullptr, &mod1g1, 1,
        FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);
    fizzy_free_instance(instance);

    // not enough functions
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(fizzy_resolve_instantiate(module, host_funcs, 3, nullptr, nullptr, &mod1g1, 1,
                  FizzyMemoryPagesLimitDefault, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "imported function mod2.foo2 is required");
}

TEST(capi, resolve_instantiate_function_duplicate)
{
    /* wat2wasm
      (func (import "mod1" "foo1") (result i32))
      (func (import "mod1" "foo1") (result i32))
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f021902046d6f643104666f6f310000046d6f643104666f6f310000");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyExternalFn host_fn = [](void*, FizzyInstance*, const FizzyValue*,
                                  FizzyExecutionContext*) noexcept {
        return FizzyExecutionResult{false, true, FizzyValue{42}};
    };

    FizzyExternalFunction mod1foo1 = {
        {FizzyValueTypeI32, nullptr, 0}, nullptr, 0, host_fn, nullptr};
    FizzyImportedFunction host_funcs[] = {{"mod1", "foo1", mod1foo1}};

    auto instance = fizzy_resolve_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr), CResult(42_u32));

    fizzy_free_instance(instance);
}

TEST(capi, resolve_instantiate_globals)
{
    /* wat2wasm
      (global (import "mod1" "g1") i32)
      (global (import "mod1" "g2") (mut i32))
      (global (import "mod2" "g1") i64)
      (global (import "mod2" "g2") (mut i64))
      (func (import "mod1" "foo1")) ;; just to test combination with other import types
      (func (result i32) (global.get 0))
      (func (result i32) (global.get 1))
      (func (result i64) (global.get 2))
      (func (result i64) (global.get 3))
   */
    const auto wasm = from_hex(
        "0061736d01000000010c036000006000017f6000017e023905046d6f6431026731037f00046d6f643102673203"
        "7f01046d6f6432026731037e00046d6f6432026732037e01046d6f643104666f6f310000030504010102020a15"
        "04040023000b040023010b040023020b040023030b");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyError error;
    EXPECT_EQ(fizzy_resolve_instantiate(module, nullptr, 0, nullptr, nullptr, nullptr, 0,
                  FizzyMemoryPagesLimitDefault, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "imported function mod1.foo1 is required");

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyExternalFn host_fn = [](void*, FizzyInstance*, const FizzyValue*,
                                  FizzyExecutionContext*) noexcept {
        return FizzyExecutionResult{true, false, {0}};
    };
    FizzyImportedFunction mod1foo1 = {
        "mod1", "foo1", {{FizzyValueTypeVoid, nullptr, 0}, nullptr, 0, host_fn, nullptr}};

    FizzyValue mod1g1value{42};
    FizzyExternalGlobal mod1g1 = {&mod1g1value, {FizzyValueTypeI32, false}};
    FizzyValue mod1g2value{43};
    FizzyExternalGlobal mod1g2 = {&mod1g2value, {FizzyValueTypeI32, true}};
    FizzyValue mod2g1value{44};
    FizzyExternalGlobal mod2g1 = {&mod2g1value, {FizzyValueTypeI64, false}};
    FizzyValue mod2g2value{45};
    FizzyExternalGlobal mod2g2 = {&mod2g2value, {FizzyValueTypeI64, true}};

    FizzyImportedGlobal host_globals[] = {{"mod1", "g1", mod1g1}, {"mod1", "g2", mod1g2},
        {"mod2", "g1", mod2g1}, {"mod2", "g2", mod2g2}};

    auto instance = fizzy_resolve_instantiate(module, &mod1foo1, 1, nullptr, nullptr, host_globals,
        4, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 2, nullptr), CResult(43_u32));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr), CResult(44_u64));
    EXPECT_THAT(fizzy_execute(instance, 4, nullptr), CResult(45_u64));

    fizzy_free_instance(instance);

    // reordered globals
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    FizzyImportedGlobal host_globals_reordered[] = {{"mod1", "g2", mod1g2}, {"mod2", "g1", mod2g1},
        {"mod2", "g2", mod2g2}, {"mod1", "g1", mod1g1}};
    instance = fizzy_resolve_instantiate(module, &mod1foo1, 1, nullptr, nullptr,
        host_globals_reordered, 4, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 2, nullptr), CResult(43_u32));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr), CResult(44_u64));
    EXPECT_THAT(fizzy_execute(instance, 4, nullptr), CResult(45_u64));

    fizzy_free_instance(instance);

    // extra globals
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    FizzyImportedGlobal host_globals_extra[] = {{"mod1", "g1", mod1g1}, {"mod1", "g2", mod1g2},
        {"mod2", "g1", mod2g1}, {"mod2", "g2", mod2g2}, {"mod3", "g1", mod1g1}};
    instance = fizzy_resolve_instantiate(module, &mod1foo1, 1, nullptr, nullptr, host_globals_extra,
        4, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 2, nullptr), CResult(43_u32));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr), CResult(44_u64));
    EXPECT_THAT(fizzy_execute(instance, 4, nullptr), CResult(45_u64));

    fizzy_free_instance(instance);

    // not enough globals
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    EXPECT_EQ(fizzy_resolve_instantiate(module, &mod1foo1, 1, nullptr, nullptr, host_globals_extra,
                  3, FizzyMemoryPagesLimitDefault, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "imported global mod2.g2 is required");
}

TEST(capi, resolve_instantiate_global_duplicate)
{
    /* wat2wasm
      (global (import "mod1" "g1") i32)
      (global (import "mod1" "g1") i32)
      (func (result i32) (global.get 0))
      (func (result i32) (global.get 1))
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f021702046d6f6431026731037f00046d6f6431026731037f000303020000"
        "0a0b02040023000b040023010b");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyValue mod1g1value{42};
    FizzyExternalGlobal mod1g1 = {&mod1g1value, {FizzyValueTypeI32, false}};

    FizzyImportedGlobal host_globals[] = {{"mod1", "g1", mod1g1}};

    auto instance = fizzy_resolve_instantiate(module, nullptr, 0, nullptr, nullptr, host_globals, 1,
        FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr), CResult(42_u32));

    fizzy_free_instance(instance);
}

TEST(capi, resolve_instantiate_custom_hard_memory_limit)
{
    /* wat2wasm
      (memory 2)
    */
    const auto wasm = from_hex("0061736d010000000503010002");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    uint32_t memory_pages_limit = 2;
    auto instance = fizzy_resolve_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, memory_pages_limit, nullptr);
    EXPECT_NE(instance, nullptr);
    fizzy_free_instance(instance);

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    memory_pages_limit = 1;
    FizzyError error;
    EXPECT_EQ(fizzy_resolve_instantiate(
                  module, nullptr, 0, nullptr, nullptr, nullptr, 0, memory_pages_limit, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "cannot exceed hard memory limit of 65536 bytes");

    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    memory_pages_limit = std::numeric_limits<uint32_t>::max();
    EXPECT_EQ(fizzy_resolve_instantiate(
                  module, nullptr, 0, nullptr, nullptr, nullptr, 0, memory_pages_limit, &error),
        nullptr);
    EXPECT_EQ(error.code, FizzyErrorInstantiationFailed);
    EXPECT_STREQ(error.message, "hard memory limit cannot exceed 4294967296 bytes");
}

TEST(capi, free_instance_null)
{
    fizzy_free_instance(nullptr);
}

TEST(capi, get_instance_module)
{
    /* wat2wasm
      (func (param i32 i32))
    */
    const auto wasm = from_hex("0061736d0100000001060160027f7f00030201000a040102000b");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    auto instance_module = fizzy_get_instance_module(instance);
    ASSERT_NE(instance_module, nullptr);

    EXPECT_EQ(fizzy_get_function_type(instance_module, 0).inputs_size, 2);

    fizzy_free_instance(instance);
}

TEST(capi, memory_access_no_memory)
{
    /* wat2wasm
      (module)
    */
    const auto wasm = from_hex("0061736d01000000");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_EQ(fizzy_get_instance_memory_data(instance), nullptr);
    EXPECT_EQ(fizzy_get_instance_memory_size(instance), 0);

    fizzy_free_instance(instance);
}

TEST(capi, memory_access_empty_memory)
{
    /* wat2wasm
      (memory 0)
    */
    const auto wasm = from_hex("0061736d010000000503010000");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_NE(fizzy_get_instance_memory_data(instance), nullptr);
    EXPECT_EQ(fizzy_get_instance_memory_size(instance), 0);

    fizzy_free_instance(instance);
}

TEST(capi, memory_access)
{
    /* wat2wasm
      (memory 1)
      (data (i32.const 1) "\11\22")
      (func (result i32)
        i32.const 0
        i32.load
      )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f0302010005030100010a0901070041002802000b0b08010041010b02112"
        "2");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    uint8_t* memory = fizzy_get_instance_memory_data(instance);
    ASSERT_NE(memory, nullptr);
    EXPECT_EQ(memory[0], 0);
    EXPECT_EQ(memory[1], 0x11);
    EXPECT_EQ(memory[2], 0x22);
    EXPECT_EQ(fizzy_get_instance_memory_size(instance), 65536);

    memory[0] = 0xaa;
    memory[1] = 0xbb;

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr), CResult(0x22bbaa_u32));

    fizzy_free_instance(instance);
}

TEST(capi, imported_memory_access)
{
    /* wat2wasm
      (memory (export "mem") 1)
      (data (i32.const 1) "\11\22")
      (func (result i32)
        i32.const 0
        i32.load
      )
    */
    const auto wasm_memory = from_hex(
        "0061736d010000000105016000017f030201000503010001070701036d656d02000a0901070041002802000b0b"
        "08010041010b021122");
    auto* module_memory = fizzy_parse(wasm_memory.data(), wasm_memory.size(), nullptr);
    ASSERT_NE(module_memory, nullptr);
    auto* instance_memory = fizzy_instantiate(module_memory, nullptr, 0, nullptr, nullptr, nullptr,
        0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance_memory, nullptr);

    FizzyExternalMemory memory;
    ASSERT_TRUE(fizzy_find_exported_memory(instance_memory, "mem", &memory));

    /* wat2wasm
      (memory (import "mod" "mem") 1)
      (func (result i32)
        i32.const 0
        i32.load
      )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f020c01036d6f64036d656d020001030201000a0901070041002802000b");
    auto* module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto* instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, &memory, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_EQ(fizzy_execute(instance, 0, nullptr).value.i32, 0x221100);

    EXPECT_EQ(fizzy_get_instance_memory_size(instance), 65536);

    uint8_t* memory_data = fizzy_get_instance_memory_data(instance);
    ASSERT_NE(memory_data, nullptr);

    memory_data[0] = 0xaa;
    memory_data[1] = 0xbb;

    EXPECT_EQ(fizzy_execute(instance_memory, 0, nullptr).value.i32, 0x22bbaa);
    EXPECT_EQ(fizzy_execute(instance, 0, nullptr).value.i32, 0x22bbaa);

    fizzy_free_instance(instance);
    fizzy_free_instance(instance_memory);
}

TEST(capi, execute)
{
    /* wat2wasm
      (func)
      (func (result i32) i32.const 42)
      (func (param i32 i32) (result i32)
        (i32.div_u (local.get 0) (local.get 1))
      )
      (func unreachable)
    */
    const auto wasm = from_hex(
        "0061736d01000000010e036000006000017f60027f7f017f030504000102000a150402000b0400412a0b070020"
        "0020016e0b0300000b");

    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    auto instance = fizzy_instantiate(
        module, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr), CResult());
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr), CResult(42_u32));
    FizzyValue args[] = {{42}, {2}};
    EXPECT_THAT(fizzy_execute(instance, 2, args), CResult(21_u32));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr), CTraps());

    fizzy_free_instance(instance);
}

TEST(capi, execute_with_host_function)
{
    /* wat2wasm
      (func (import "mod1" "foo1") (result i32))
      (func (import "mod1" "foo2") (param i32 i32) (result i32))
    */
    const auto wasm = from_hex(
        "0061736d01000000010b026000017f60027f7f017f021902046d6f643104666f6f310000046d6f643104666f6f"
        "320001");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    const FizzyValueType inputs[] = {FizzyValueTypeI32, FizzyValueTypeI32};

    FizzyExternalFunction host_funcs[] = {
        {{FizzyValueTypeI32, nullptr, 0}, nullptr, 0,
            [](void*, FizzyInstance*, const FizzyValue*, FizzyExecutionContext*) noexcept {
                return FizzyExecutionResult{false, true, {42}};
            },
            nullptr},
        {{FizzyValueTypeI32, &inputs[0], 2}, nullptr, 0,
            [](void*, FizzyInstance*, const FizzyValue* args, FizzyExecutionContext*) noexcept {
                FizzyValue v;
                v.i32 = args[0].i32 / args[1].i32;
                return FizzyExecutionResult{false, true, {v}};
            },
            nullptr}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 2, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr), CResult(42_u32));

    FizzyValue args[] = {{42}, {2}};
    EXPECT_THAT(fizzy_execute(instance, 1, args), CResult(21_u32));

    fizzy_free_instance(instance);
}

TEST(capi, imported_function_traps)
{
    /* wat2wasm
      (func (import "m" "foo") (result i32))
      (func (result i32)
        call 0
      )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020901016d03666f6f0000030201000a0601040010000b");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    FizzyExternalFunction host_funcs[] = {{{FizzyValueTypeI32, nullptr, 0}, nullptr, 0,
        [](void*, FizzyInstance*, const FizzyValue*, FizzyExecutionContext*) noexcept {
            return FizzyExecutionResult{true, false, {}};
        },
        nullptr}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr), CTraps());

    fizzy_free_instance(instance);
}

TEST(capi, imported_function_void)
{
    /* wat2wasm
      (func (import "m" "foo"))
      (func
        call 0
      )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000020901016d03666f6f0000030201000a0601040010000b");
    auto module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);

    bool called = false;
    FizzyExternalFunction host_funcs[] = {{{}, nullptr, 0,
        [](void* context, FizzyInstance*, const FizzyValue*, FizzyExecutionContext*) noexcept {
            *static_cast<bool*>(context) = true;
            return FizzyExecutionResult{false, false, {}};
        },
        &called}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr), CResult());
    EXPECT_TRUE(called);

    fizzy_free_instance(instance);
}

TEST(capi, imported_function_from_another_module)
{
    /* wat2wasm
    (module
      (func $sub (param $lhs i32) (param $rhs i32) (result i32)
        local.get $lhs
        local.get $rhs
        i32.sub)
      (export "sub" (func $sub))
    )
    */
    const auto bin1 = from_hex(
        "0061736d0100000001070160027f7f017f030201000707010373756200000a09010700200020016b0b");
    auto module1 = fizzy_parse(bin1.data(), bin1.size(), nullptr);
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance1, nullptr);

    FizzyExternalFunction func;
    ASSERT_TRUE(fizzy_find_exported_function(instance1, "sub", &func));

    /* wat2wasm
    (module
      (func $sub (import "m1" "sub") (param $lhs i32) (param $rhs i32) (result i32))

      (func $main (param i32) (param i32) (result i32)
        local.get 0
        local.get 1
        call $sub
      )
    )
    */
    const auto bin2 = from_hex(
        "0061736d0100000001070160027f7f017f020a01026d31037375620000030201000a0a0108002000200110000"
        "b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size(), nullptr);
    ASSERT_NE(module2, nullptr);

    auto instance2 = fizzy_instantiate(
        module2, &func, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance2, nullptr);

    FizzyValue args[] = {{44}, {2}};
    EXPECT_THAT(fizzy_execute(instance2, 1, args), CResult(42_u32));

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi, imported_table_from_another_module)
{
    /* wat2wasm
      (table (export "t") 10 30 funcref)
      (elem (i32.const 1) $f) ;; Table contents: uninit, f, uninit, ...
      (func $f (result i32) (i32.const 42))
    */
    const auto bin1 = from_hex(
        "0061736d010000000105016000017f0302010004050170010a1e070501017401000907010041010b01000a0601"
        "0400412a0b");
    auto module1 = fizzy_parse(bin1.data(), bin1.size(), nullptr);
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance1, nullptr);

    /* wat2wasm
      (type (func (result i32)))
      (table (import "m1" "t") 10 30 funcref)
      (func (result i32)
        (call_indirect (type 0) (i32.const 1))
      )
    */
    const auto bin2 = from_hex(
        "0061736d010000000105016000017f020b01026d3101740170010a1e030201000a0901070041011100000b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size(), nullptr);
    ASSERT_NE(module2, nullptr);

    FizzyExternalTable table;
    ASSERT_TRUE(fizzy_find_exported_table(instance1, "t", &table));

    auto instance2 = fizzy_instantiate(
        module2, nullptr, 0, &table, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance2, nullptr);

    EXPECT_THAT(fizzy_execute(instance2, 0, nullptr), CResult(42_u32));

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi, imported_memory_from_another_module)
{
    /* wat2wasm
      (memory (export "m") 1)
      (data (i32.const 10) "\aa\ff")
    */
    const auto bin1 = from_hex("0061736d010000000503010001070501016d02000b080100410a0b02aaff");
    auto module1 = fizzy_parse(bin1.data(), bin1.size(), nullptr);
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance1, nullptr);

    /* wat2wasm
      (memory (import "m1" "m") 1)
      (func (result i32)
        (i32.const 9)
        (i32.load)
      )
    */
    const auto bin2 = from_hex(
        "0061736d010000000105016000017f020901026d31016d020001030201000a0901070041092802000b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size(), nullptr);
    ASSERT_NE(module2, nullptr);

    FizzyExternalMemory memory;
    ASSERT_TRUE(fizzy_find_exported_memory(instance1, "m", &memory));

    auto instance2 = fizzy_instantiate(
        module2, nullptr, 0, nullptr, &memory, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance2, nullptr);

    EXPECT_THAT(fizzy_execute(instance2, 0, nullptr), CResult(0x00ffaa00_u32));

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi, imported_global_from_another_module)
{
    /* wat2wasm
      (global (export "g") i32 (i32.const 42))
    */
    const auto bin1 = from_hex("0061736d010000000606017f00412a0b07050101670300");
    auto module1 = fizzy_parse(bin1.data(), bin1.size(), nullptr);
    ASSERT_NE(module1, nullptr);
    auto instance1 = fizzy_instantiate(
        module1, nullptr, 0, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance1, nullptr);

    /* wat2wasm
    (module
      (global (import "m1" "g") i32)
      (func (result i32)
        global.get 0
      )
    )
    */
    const auto bin2 =
        from_hex("0061736d010000000105016000017f020901026d310167037f00030201000a0601040023000b");
    auto module2 = fizzy_parse(bin2.data(), bin2.size(), nullptr);
    ASSERT_NE(module2, nullptr);

    FizzyExternalGlobal global;
    ASSERT_TRUE(fizzy_find_exported_global(instance1, "g", &global));

    auto instance2 = fizzy_instantiate(
        module2, nullptr, 0, nullptr, nullptr, &global, 1, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance2, nullptr);

    EXPECT_THAT(fizzy_execute(instance2, 0, nullptr), CResult(42_u32));

    fizzy_free_instance(instance2);
    fizzy_free_instance(instance1);
}

TEST(capi, get_type_count)
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

TEST(capi, get_type)
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

TEST(capi, get_import_count)
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

TEST(capi, get_import_description)
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

TEST(capi, import_name_after_instantiate)
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

    FizzyExternalFunction host_funcs[] = {
        {{FizzyValueTypeI32, nullptr, 0}, nullptr, 0, NullFn, nullptr}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    EXPECT_STREQ(import0.module, "m");
    EXPECT_STREQ(import0.name, "f1");

    fizzy_free_instance(instance);
}

TEST(capi, get_global_count)
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

TEST(capi, get_global_type)
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
