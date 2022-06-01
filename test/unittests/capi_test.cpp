// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <fizzy/fizzy.h>
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy::test;

static_assert(FizzyMemoryPagesLimitDefault == fizzy::DefaultMemoryPagesLimit);

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

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, nullptr), CResult(0x22bbaa_u32));

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

    EXPECT_EQ(fizzy_execute(instance, 0, nullptr, nullptr).value.i32, 0x221100);

    EXPECT_EQ(fizzy_get_instance_memory_size(instance), 65536);

    uint8_t* memory_data = fizzy_get_instance_memory_data(instance);
    ASSERT_NE(memory_data, nullptr);

    memory_data[0] = 0xaa;
    memory_data[1] = 0xbb;

    EXPECT_EQ(fizzy_execute(instance_memory, 0, nullptr, nullptr).value.i32, 0x22bbaa);
    EXPECT_EQ(fizzy_execute(instance, 0, nullptr, nullptr).value.i32, 0x22bbaa);

    fizzy_free_instance(instance);
    fizzy_free_instance(instance_memory);
}
