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

TEST(capi_instantiate, instantiate)
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

TEST(capi_instantiate, instantiate_imported_function)
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

    FizzyExternalFunction host_funcs[] = {{{FizzyValueTypeI32, nullptr, 0}, NullFn, nullptr}};

    auto instance = fizzy_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    fizzy_free_instance(instance);
}

TEST(capi_instantiate, instantiate_imported_globals)
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

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, nullptr), CResult(43_u64));
    EXPECT_THAT(fizzy_execute(instance, 2, nullptr, nullptr), CResult(44.4f));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr, nullptr), CResult(45.5));

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

TEST(capi_instantiate, instantiate_twice)
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

TEST(capi_instantiate, instantiate_custom_hard_memory_limit)
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

TEST(capi_instantiate, resolve_instantiate_no_imports)
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
        {"mod", "foo", {{FizzyValueTypeVoid, nullptr, 0}, NullFn, nullptr}}};

    instance = fizzy_resolve_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    fizzy_free_instance(instance);
}

TEST(capi_instantiate, resolve_instantiate_functions)
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
    FizzyExternalFunction mod1foo1 = {{FizzyValueTypeI32, &input_type, 1}, host_fn, &result_int32};
    FizzyValue result_int64{43};
    FizzyExternalFunction mod1foo2 = {{FizzyValueTypeI64, &input_type, 1}, host_fn, &result_int64};
    FizzyValue result_f32;
    result_f32.f32 = 44.44f;
    FizzyExternalFunction mod2foo1 = {{FizzyValueTypeF32, &input_type, 1}, host_fn, &result_f32};
    FizzyValue result_f64;
    result_f64.f64 = 45.45;
    FizzyExternalFunction mod2foo2 = {{FizzyValueTypeF64, &input_type, 1}, host_fn, &result_f64};

    FizzyImportedFunction host_funcs[] = {{"mod1", "foo1", mod1foo1}, {"mod1", "foo2", mod1foo2},
        {"mod2", "foo1", mod2foo1}, {"mod2", "foo2", mod2foo2}};

    auto instance = fizzy_resolve_instantiate(
        module, host_funcs, 4, nullptr, nullptr, &mod1g1, 1, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    FizzyValue arg;
    EXPECT_THAT(fizzy_execute(instance, 0, &arg, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 1, &arg, nullptr), CResult(43_u64));
    EXPECT_THAT(fizzy_execute(instance, 2, &arg, nullptr), CResult(44.44f));
    EXPECT_THAT(fizzy_execute(instance, 3, &arg, nullptr), CResult(45.45));

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

TEST(capi_instantiate, resolve_instantiate_function_duplicate)
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

    FizzyExternalFunction mod1foo1 = {{FizzyValueTypeI32, nullptr, 0}, host_fn, nullptr};
    FizzyImportedFunction host_funcs[] = {{"mod1", "foo1", mod1foo1}};

    auto instance = fizzy_resolve_instantiate(
        module, host_funcs, 1, nullptr, nullptr, nullptr, 0, FizzyMemoryPagesLimitDefault, nullptr);
    ASSERT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, nullptr), CResult(42_u32));

    fizzy_free_instance(instance);
}

TEST(capi_instantiate, resolve_instantiate_globals)
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
        "mod1", "foo1", {{FizzyValueTypeVoid, nullptr, 0}, host_fn, nullptr}};

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

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 2, nullptr, nullptr), CResult(43_u32));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr, nullptr), CResult(44_u64));
    EXPECT_THAT(fizzy_execute(instance, 4, nullptr, nullptr), CResult(45_u64));

    fizzy_free_instance(instance);

    // reordered globals
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    FizzyImportedGlobal host_globals_reordered[] = {{"mod1", "g2", mod1g2}, {"mod2", "g1", mod2g1},
        {"mod2", "g2", mod2g2}, {"mod1", "g1", mod1g1}};
    instance = fizzy_resolve_instantiate(module, &mod1foo1, 1, nullptr, nullptr,
        host_globals_reordered, 4, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 2, nullptr, nullptr), CResult(43_u32));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr, nullptr), CResult(44_u64));
    EXPECT_THAT(fizzy_execute(instance, 4, nullptr, nullptr), CResult(45_u64));

    fizzy_free_instance(instance);

    // extra globals
    module = fizzy_parse(wasm.data(), wasm.size(), nullptr);
    ASSERT_NE(module, nullptr);
    FizzyImportedGlobal host_globals_extra[] = {{"mod1", "g1", mod1g1}, {"mod1", "g2", mod1g2},
        {"mod2", "g1", mod2g1}, {"mod2", "g2", mod2g2}, {"mod3", "g1", mod1g1}};
    instance = fizzy_resolve_instantiate(module, &mod1foo1, 1, nullptr, nullptr, host_globals_extra,
        4, FizzyMemoryPagesLimitDefault, nullptr);
    EXPECT_NE(instance, nullptr);

    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 2, nullptr, nullptr), CResult(43_u32));
    EXPECT_THAT(fizzy_execute(instance, 3, nullptr, nullptr), CResult(44_u64));
    EXPECT_THAT(fizzy_execute(instance, 4, nullptr, nullptr), CResult(45_u64));

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

TEST(capi_instantiate, resolve_instantiate_global_duplicate)
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

    EXPECT_THAT(fizzy_execute(instance, 0, nullptr, nullptr), CResult(42_u32));
    EXPECT_THAT(fizzy_execute(instance, 1, nullptr, nullptr), CResult(42_u32));

    fizzy_free_instance(instance);
}

TEST(capi_instantiate, resolve_instantiate_custom_hard_memory_limit)
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

TEST(capi_instantiate, free_instance_null)
{
    fizzy_free_instance(nullptr);
}

TEST(capi_instantiate, get_instance_module)
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
