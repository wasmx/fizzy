// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <lib/fizzy/limits.hpp>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

namespace
{
ExecuteFunction function_returning_value(Value value)
{
    static constexpr HostFunctionPtr func = [](std::any& host_context, Instance&, const Value*,
                                                int) noexcept -> ExecutionResult {
        return *std::any_cast<Value>(&host_context);
    };

    return {func, std::make_any<Value>(value)};
}

ExecutionResult function_returning_void(std::any&, Instance&, const Value*, int) noexcept
{
    return Void;
}
}  // namespace

TEST(api, execution_result_trap)
{
    const auto result = Trap;
    EXPECT_TRUE(result.trapped);
    EXPECT_FALSE(result.has_value);
    EXPECT_EQ(result.value.i64, 0);
}

TEST(api, execution_result_void)
{
    const auto result = Void;
    EXPECT_FALSE(result.trapped);
    EXPECT_FALSE(result.has_value);
    EXPECT_EQ(result.value.i64, 0);
}

TEST(api, execution_result_value)
{
    const ExecutionResult result = Value{1234_u32};
    EXPECT_FALSE(result.trapped);
    EXPECT_TRUE(result.has_value);
    EXPECT_EQ(result.value.i32, 1234_u32);
}

TEST(api, execution_result_bool_constructor)
{
    bool success = false;
    const ExecutionResult result{success};
    EXPECT_TRUE(result.trapped);
    EXPECT_FALSE(result.has_value);
    EXPECT_EQ(result.value.i64, 0);
}

TEST(api, execution_result_value_constructor)
{
    Value value{1234_u32};
    const ExecutionResult result{value};
    EXPECT_FALSE(result.trapped);
    EXPECT_TRUE(result.has_value);
    EXPECT_EQ(result.value.i32, 1234_u32);
}

TEST(api, resolve_imported_functions)
{
    /* wat2wasm
      (func (import "mod1" "foo1") (result i32))
      (func (import "mod1" "foo2") (param i32) (result i32))
      (func (import "mod2" "foo1") (param i32) (result i32))
      (func (import "mod2" "foo2") (param i64) (param i32))
      (global (import "mod1" "g") i32) ;; just to test combination with other import types
    */
    const auto wasm = from_hex(
        "0061736d01000000010f036000017f60017f017f60027e7f00023b05046d6f643104666f6f310000046d6f6431"
        "04666f6f320001046d6f643204666f6f310001046d6f643204666f6f320002046d6f64310167037f00");
    const auto module = parse(wasm);

    const std::vector<ImportedFunction> imported_functions = {
        {"mod1", "foo1", {}, ValType::i32, function_returning_value(0)},
        {"mod1", "foo2", {ValType::i32}, ValType::i32, function_returning_value(1)},
        {"mod2", "foo1", {ValType::i32}, ValType::i32, function_returning_value(2)},
        {"mod2", "foo2", {ValType::i64, ValType::i32}, std::nullopt, function_returning_void},
    };

    const auto external_functions = resolve_imported_functions(*module, imported_functions);

    EXPECT_EQ(external_functions.size(), 4);

    Value global = 0;
    const std::vector<ExternalGlobal> external_globals{{&global, {ValType::i32, false}}};
    auto instance = instantiate(
        *module, external_functions, {}, {}, std::vector<ExternalGlobal>(external_globals));

    EXPECT_THAT(execute(*instance, 0, {}), Result(0));
    EXPECT_THAT(execute(*instance, 1, {0_u32}), Result(1));
    EXPECT_THAT(execute(*instance, 2, {0_u32}), Result(2));
    EXPECT_THAT(execute(*instance, 3, {0_u64, 0_u32}), Result());


    const std::vector<ImportedFunction> imported_functions_reordered = {
        {"mod2", "foo1", {ValType::i32}, ValType::i32, function_returning_value(2)},
        {"mod1", "foo2", {ValType::i32}, ValType::i32, function_returning_value(1)},
        {"mod1", "foo1", {}, ValType::i32, function_returning_value(0)},
        {"mod2", "foo2", {ValType::i64, ValType::i32}, std::nullopt, function_returning_void},
    };

    const auto external_functions_reordered =
        resolve_imported_functions(*module, imported_functions_reordered);
    EXPECT_EQ(external_functions_reordered.size(), 4);

    auto instance_reordered =
        instantiate(*module, external_functions_reordered, {}, {}, external_globals);

    EXPECT_THAT(execute(*instance_reordered, 0, {}), Result(0));
    EXPECT_THAT(execute(*instance_reordered, 1, {0_u32}), Result(1));
    EXPECT_THAT(execute(*instance_reordered, 2, {0_u32}), Result(2));
    EXPECT_THAT(execute(*instance_reordered, 3, {0_u64, 0_u32}), Result());


    const std::vector<ImportedFunction> imported_functions_extra = {
        {"mod1", "foo1", {}, ValType::i32, function_returning_value(0)},
        {"mod1", "foo2", {ValType::i32}, ValType::i32, function_returning_value(1)},
        {"mod2", "foo1", {ValType::i32}, ValType::i32, function_returning_value(2)},
        {"mod2", "foo2", {ValType::i64, ValType::i32}, std::nullopt, function_returning_void},
        {"mod3", "foo1", {}, std::nullopt, function_returning_value(4)},
        {"mod3", "foo2", {}, std::nullopt, function_returning_value(5)},
    };

    const auto external_functions_extra =
        resolve_imported_functions(*module, imported_functions_extra);
    EXPECT_EQ(external_functions_extra.size(), 4);

    auto instance_extra = instantiate(*module, external_functions_extra, {}, {}, external_globals);

    EXPECT_THAT(execute(*instance_extra, 0, {}), Result(0));
    EXPECT_THAT(execute(*instance_extra, 1, {0_u32}), Result(1));
    EXPECT_THAT(execute(*instance_extra, 2, {0_u32}), Result(2));
    EXPECT_THAT(execute(*instance_extra, 3, {0_u64, 0_u32}), Result());


    const std::vector<ImportedFunction> imported_functions_missing = {
        {"mod1", "foo1", {}, ValType::i32, function_returning_value(0)},
        {"mod1", "foo2", {ValType::i32}, ValType::i32, function_returning_value(1)},
        {"mod2", "foo1", {ValType::i32}, ValType::i32, function_returning_value(2)},
    };

    EXPECT_THROW_MESSAGE(resolve_imported_functions(*module, imported_functions_missing),
        instantiate_error, "imported function mod2.foo2 is required");


    const std::vector<ImportedFunction> imported_functions_invalid_type1 = {
        {"mod1", "foo1", {ValType::i32}, ValType::i32, function_returning_value(0)},
        {"mod1", "foo2", {ValType::i32}, ValType::i32, function_returning_value(1)},
        {"mod2", "foo1", {ValType::i32}, ValType::i32, function_returning_value(2)},
        {"mod2", "foo2", {ValType::i64, ValType::i32}, std::nullopt, function_returning_void},
    };

    EXPECT_THROW_MESSAGE(resolve_imported_functions(*module, imported_functions_invalid_type1),
        instantiate_error,
        "function mod1.foo1 input types don't match imported function in module");

    const std::vector<ImportedFunction> imported_functions_invalid_type2 = {
        {"mod1", "foo1", {}, ValType::i32, function_returning_value(0)},
        {"mod1", "foo2", {ValType::i32}, ValType::i32, function_returning_value(1)},
        {"mod2", "foo1", {ValType::i32}, ValType::i32, function_returning_value(2)},
        {"mod2", "foo2", {ValType::i64, ValType::i32}, ValType::i64, function_returning_value(3)},
    };

    EXPECT_THROW_MESSAGE(resolve_imported_functions(*module, imported_functions_invalid_type2),
        instantiate_error, "function mod2.foo2 has output but is defined void in module");

    const std::vector<ImportedFunction> imported_functions_invalid_type3 = {
        {"mod1", "foo1", {}, ValType::i32, function_returning_value(0)},
        {"mod1", "foo2", {ValType::i32}, ValType::i64, function_returning_value(1)},
        {"mod2", "foo1", {ValType::i32}, ValType::i32, function_returning_value(2)},
        {"mod2", "foo2", {ValType::i64, ValType::i32}, std::nullopt, function_returning_void},
    };

    EXPECT_THROW_MESSAGE(resolve_imported_functions(*module, imported_functions_invalid_type3),
        instantiate_error,
        "function mod1.foo2 output type doesn't match imported function in module");
}

TEST(api, resolve_imported_function_duplicate)
{
    /* wat2wasm
      (func (import "mod1" "foo1") (param i32) (result i32))
      (func (import "mod1" "foo1") (param i32) (result i32))
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f021902046d6f643104666f6f310000046d6f643104666f6f310000");
    const auto module = parse(wasm);

    std::vector<ImportedFunction> imported_functions = {
        {"mod1", "foo1", {ValType::i32}, ValType::i32, function_returning_value(42)},
    };

    const auto external_functions = resolve_imported_functions(*module, imported_functions);

    EXPECT_EQ(external_functions.size(), 2);

    auto instance = instantiate(*module, external_functions, {}, {}, {});

    EXPECT_THAT(execute(*instance, 0, {0_u32}), Result(42));
    EXPECT_THAT(execute(*instance, 1, {0_u32}), Result(42));
}

TEST(api, resolve_imported_function_duplicate_with_context)
{
    /* wat2wasm
      (func (import "mod1" "foo1"))
      (func (import "mod1" "foo1"))
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000021902046d6f643104666f6f310000046d6f643104666f6f310000");
    const auto module = parse(wasm);

    constexpr auto host_func = [](std::any& context, Instance&, const Value*, int) noexcept {
        auto* counter = *std::any_cast<int*>(&context);
        ++(*counter);
        return Void;
    };

    int counter = 0;
    auto context = std::make_any<int*>(&counter);

    std::vector<ImportedFunction> imported_functions = {
        {"mod1", "foo1", {}, std::nullopt, {host_func, std::move(context)}},
    };

    const auto external_functions = resolve_imported_functions(*module, imported_functions);

    EXPECT_EQ(external_functions.size(), 2);

    auto instance = instantiate(*module, external_functions, {}, {}, {});

    EXPECT_THAT(execute(*instance, 0, nullptr), Result());
    EXPECT_THAT(execute(*instance, 1, nullptr), Result());
    EXPECT_EQ(counter, 2);
}

TEST(api, resolve_imported_globals)
{
    /* wat2wasm
      (global (import "mod1" "g1") i32)
      (global (import "mod1" "g2") (mut i32))
      (global (import "mod2" "g1") i64)
      (global (import "mod2" "g2") (mut i64))
      (func (import "mod1" "foo1")
        (param i32) (result i32)) ;; just to test combination with other import types
      (func (result i32) (global.get 0))
      (func (result i32) (global.get 1))
      (func (result i64) (global.get 2))
      (func (result i64) (global.get 3))
   */
    const auto wasm = from_hex(
        "0061736d01000000010e0360017f017f6000017f6000017e023905046d6f6431026731037f00046d6f64310267"
        "32037f01046d6f6432026731037e00046d6f6432026732037e01046d6f643104666f6f31000003050401010202"
        "0a1504040023000b040023010b040023020b040023030b");
    const auto module = parse(wasm);

    std::vector<ImportedFunction> imported_functions = {
        {"mod1", "foo1", {ValType::i32}, ValType::i32, function_returning_value(1)},
    };
    const auto external_functions = resolve_imported_functions(*module, imported_functions);
    EXPECT_EQ(external_functions.size(), 1);

    Value global1{42};
    Value global2{43};
    Value global3{44};
    Value global4{45};
    const std::vector<ImportedGlobal> imported_globals = {
        {"mod1", "g1", &global1, ValType::i32, false},
        {"mod1", "g2", &global2, ValType::i32, true},
        {"mod2", "g1", &global3, ValType::i64, false},
        {"mod2", "g2", &global4, ValType::i64, true},
    };

    const auto external_globals = resolve_imported_globals(*module, imported_globals);
    EXPECT_EQ(external_globals.size(), 4);

    auto instance = instantiate(*module, external_functions, {}, {}, external_globals);

    EXPECT_THAT(execute(*instance, 1, {}), Result(42));
    EXPECT_THAT(execute(*instance, 2, {}), Result(43));
    EXPECT_THAT(execute(*instance, 3, {}), Result(44));
    EXPECT_THAT(execute(*instance, 4, {}), Result(45));

    const std::vector<ImportedGlobal> imported_globals_reordered = {
        {"mod2", "g1", &global3, ValType::i64, false},
        {"mod1", "g2", &global2, ValType::i32, true},
        {"mod1", "g1", &global1, ValType::i32, false},
        {"mod2", "g2", &global4, ValType::i64, true},
    };
    const auto external_globals_reordered =
        resolve_imported_globals(*module, imported_globals_reordered);
    EXPECT_EQ(external_globals_reordered.size(), 4);

    auto instance_reordered =
        instantiate(*module, external_functions, {}, {}, external_globals_reordered);

    EXPECT_THAT(execute(*instance_reordered, 1, {}), Result(42));
    EXPECT_THAT(execute(*instance_reordered, 2, {}), Result(43));
    EXPECT_THAT(execute(*instance_reordered, 3, {}), Result(44));
    EXPECT_THAT(execute(*instance_reordered, 4, {}), Result(45));

    Value global5{46};
    Value global6{47};
    const std::vector<ImportedGlobal> imported_globals_extra = {
        {"mod1", "g1", &global1, ValType::i32, false},
        {"mod1", "g2", &global2, ValType::i32, true},
        {"mod2", "g1", &global3, ValType::i64, false},
        {"mod2", "g2", &global4, ValType::i64, true},
        {"mod3", "g1", &global5, ValType::i32, false},
        {"mod3", "g2", &global6, ValType::i32, true},
    };

    const auto external_globals_extra = resolve_imported_globals(*module, imported_globals_extra);
    EXPECT_EQ(external_globals_extra.size(), 4);

    auto instance_extra = instantiate(*module, external_functions, {}, {}, external_globals_extra);

    EXPECT_THAT(execute(*instance_reordered, 1, {}), Result(42));
    EXPECT_THAT(execute(*instance_reordered, 2, {}), Result(43));
    EXPECT_THAT(execute(*instance_reordered, 3, {}), Result(44));
    EXPECT_THAT(execute(*instance_reordered, 4, {}), Result(45));

    const std::vector<ImportedGlobal> imported_globals_missing = {
        {"mod2", "g1", &global3, ValType::i64, false},
        {"mod1", "g2", &global2, ValType::i32, true},
        {"mod1", "g1", &global1, ValType::i32, false},
    };

    EXPECT_THROW_MESSAGE(resolve_imported_globals(*module, imported_globals_missing),
        instantiate_error, "imported global mod2.g2 is required");

    const std::vector<ImportedGlobal> imported_globals_invalid_type = {
        {"mod1", "g1", &global1, ValType::i64, false},
        {"mod1", "g2", &global2, ValType::i32, true},
        {"mod2", "g1", &global3, ValType::i64, false},
        {"mod2", "g2", &global4, ValType::i64, true},
    };

    EXPECT_THROW_MESSAGE(resolve_imported_globals(*module, imported_globals_invalid_type),
        instantiate_error, "global mod1.g1 value type doesn't match imported global in module");

    const std::vector<ImportedGlobal> imported_globals_invalid_mutability = {
        {"mod1", "g1", &global1, ValType::i32, true},
        {"mod1", "g2", &global2, ValType::i32, true},
        {"mod2", "g1", &global3, ValType::i64, false},
        {"mod2", "g2", &global4, ValType::i64, true},
    };

    EXPECT_THROW_MESSAGE(resolve_imported_globals(*module, imported_globals_invalid_mutability),
        instantiate_error, "global mod1.g1 mutability doesn't match imported global in module");
}

TEST(api, resolve_imported_global_duplicate)
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
    const auto module = parse(wasm);

    Value global1{42};
    const std::vector<ImportedGlobal> imported_globals = {
        {"mod1", "g1", &global1, ValType::i32, false},
    };

    const auto external_globals = resolve_imported_globals(*module, imported_globals);

    EXPECT_EQ(external_globals.size(), 2);

    auto instance = instantiate(*module, {}, {}, {}, external_globals);

    EXPECT_THAT(execute(*instance, 0, {}), Result(42));
    EXPECT_THAT(execute(*instance, 1, {}), Result(42));
}

TEST(api, find_exported_function_index)
{
    Module module;
    module.exportsec.emplace_back(Export{"foo1", ExternalKind::Function, 0});
    module.exportsec.emplace_back(Export{"foo2", ExternalKind::Function, 1});
    module.exportsec.emplace_back(Export{"foo3", ExternalKind::Function, 2});
    module.exportsec.emplace_back(Export{"foo4", ExternalKind::Function, 42});
    module.exportsec.emplace_back(Export{"mem", ExternalKind::Memory, 0});
    module.exportsec.emplace_back(Export{"glob", ExternalKind::Global, 0});
    module.exportsec.emplace_back(Export{"table", ExternalKind::Table, 0});

    auto optionalIdx = find_exported_function(module, "foo1");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 0);

    optionalIdx = find_exported_function(module, "foo2");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 1);

    optionalIdx = find_exported_function(module, "foo3");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 2);

    optionalIdx = find_exported_function(module, "foo4");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 42);

    EXPECT_FALSE(find_exported_function(module, "foo5"));
    EXPECT_FALSE(find_exported_function(module, "mem"));
    EXPECT_FALSE(find_exported_function(module, "glob"));
    EXPECT_FALSE(find_exported_function(module, "table"));
}

TEST(api, find_exported_function)
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

    auto instance = instantiate(parse(wasm));

    auto opt_function = find_exported_function(*instance, "foo");
    ASSERT_TRUE(opt_function);
    EXPECT_THAT(TypedExecutionResult(opt_function->function(*instance, {}, 0), ValType::i32),
        Result(42_u32));
    EXPECT_TRUE(opt_function->input_types.empty());
    ASSERT_EQ(opt_function->output_types.size(), 1);
    EXPECT_EQ(opt_function->output_types[0], ValType::i32);

    EXPECT_FALSE(find_exported_function(*instance, "bar").has_value());

    /* wat2wasm
    (module
      (func (export "foo") (import "spectest" "bar") (result i32))
      (global (export "g") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm_reexported_function = from_hex(
        "0061736d010000000105016000017f021001087370656374657374036261720000040401700000050401010102"
        "0606017f0041000b07170403666f6f000001670300037461620100036d656d0200");

    constexpr auto bar = [](std::any&, Instance&, const Value*, int) noexcept -> ExecutionResult {
        return Value{42};
    };
    const auto bar_type = FuncType{{}, {ValType::i32}};

    auto instance_reexported_function =
        instantiate(parse(wasm_reexported_function), {{{bar}, bar_type}});

    auto opt_reexported_function = find_exported_function(*instance_reexported_function, "foo");
    ASSERT_TRUE(opt_reexported_function);
    EXPECT_THAT(
        TypedExecutionResult(opt_reexported_function->function(*instance, {}, 0), ValType::i32),
        Result(42_u32));
    EXPECT_TRUE(opt_reexported_function->input_types.empty());
    ASSERT_EQ(opt_reexported_function->output_types.size(), 1);
    EXPECT_EQ(opt_reexported_function->output_types[0], ValType::i32);

    EXPECT_FALSE(find_exported_function(*instance, "bar").has_value());

    /* wat2wasm
    (module
      (global (export "g") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm_no_function = from_hex(
        "0061736d010000000404017000000504010101020606017f0041000b07110301670300037461620100036d656d"
        "0200");

    auto instance_no_function = instantiate(parse(wasm_no_function));

    EXPECT_FALSE(find_exported_function(*instance_no_function, "mem").has_value());
}

TEST(api, find_exported_global)
{
    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (global (export "g1") (mut i32) (i32.const 0))
      (global (export "g2") i64 (i64.const 1))
      (global (export "g3") (mut f32) (f32.const 2.345))
      (global (export "g4") f64 (f64.const 3.456))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 0)
    )
     */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000404017000000503010000061f047f0141000b7e0042010b7d0143"
        "7b1416400b7c0044d9cef753e3a50b400b07250701660000026731030002673203010267330302026734030303"
        "7461620100036d656d02000a05010300010b");

    auto instance = instantiate(parse(wasm));

    auto opt_global = find_exported_global(*instance, "g1");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(opt_global->value->i32, 0);
    EXPECT_EQ(opt_global->type.value_type, ValType::i32);
    EXPECT_TRUE(opt_global->type.is_mutable);

    opt_global = find_exported_global(*instance, "g2");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(opt_global->value->i64, 1);
    EXPECT_EQ(opt_global->type.value_type, ValType::i64);
    EXPECT_FALSE(opt_global->type.is_mutable);

    opt_global = find_exported_global(*instance, "g3");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(opt_global->value->f32, 2.345f);
    EXPECT_EQ(opt_global->type.value_type, ValType::f32);
    EXPECT_TRUE(opt_global->type.is_mutable);

    opt_global = find_exported_global(*instance, "g4");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(opt_global->value->f64, 3.456);
    EXPECT_EQ(opt_global->type.value_type, ValType::f64);
    EXPECT_FALSE(opt_global->type.is_mutable);

    EXPECT_FALSE(find_exported_global(*instance, "g5"));
    EXPECT_FALSE(find_exported_global(*instance, "f"));
    EXPECT_FALSE(find_exported_global(*instance, "tab"));
    EXPECT_FALSE(find_exported_global(*instance, "mem"));

    /* wat2wasm
    (module
      (global (export "g1") (import "test" "g2") i32)
      (global (export "g2") (mut i64) (i64.const 1))
      (table (export "tab") 0 anyfunc)
      (func (export "f") nop)
      (memory (export "mem") 0)
    )
     */
    const auto wasm_reexported_global = from_hex(
        "0061736d01000000010401600000020c010474657374026732037f000302010004040170000005030100000606"
        "017e0142010b071b050267310300026732030103746162010001660000036d656d02000a05010300010b");

    Value g1 = 42;
    auto instance_reexported_global = instantiate(
        parse(wasm_reexported_global), {}, {}, {}, {ExternalGlobal{&g1, {ValType::i32, false}}});

    opt_global = find_exported_global(*instance_reexported_global, "g1");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(opt_global->value, &g1);
    EXPECT_EQ(opt_global->type.value_type, ValType::i32);
    EXPECT_FALSE(opt_global->type.is_mutable);

    opt_global = find_exported_global(*instance_reexported_global, "g2");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(opt_global->value->i64, 1);
    EXPECT_EQ(opt_global->type.value_type, ValType::i64);
    EXPECT_TRUE(opt_global->type.is_mutable);

    EXPECT_FALSE(find_exported_global(*instance_reexported_global, "g3").has_value());

    /* wat2wasm
    (module
      (table (export "tab") 0 anyfunc)
      (func (export "f") nop)
      (memory (export "mem") 0)
    )
     */
    const auto wasm_no_globals = from_hex(
        "0061736d0100000001040160000003020100040401700000050301000007110303746162010001660000036d65"
        "6d02000a05010300010b");

    auto instance_no_globals = instantiate(parse(wasm_no_globals));

    EXPECT_FALSE(find_exported_global(*instance_no_globals, "g1").has_value());
}

TEST(api, find_exported_table)
{
    /* wat2wasm
    (module
      (func $f (export "f") (result i32) (i32.const 1))
      (func $g (result i32) (i32.const 2))
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 2 20 anyfunc)
      (elem 0 (i32.const 0) $g $f)
      (memory (export "mem") 0)
    )
     */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f03030200000405017001021405030100000606017f0041000b0716040166"
        "00000267310300037461620100036d656d02000908010041000b0201000a0b02040041010b040041020b");

    auto instance = instantiate(parse(wasm));

    auto opt_table = find_exported_table(*instance, "tab");
    ASSERT_TRUE(opt_table);
    EXPECT_EQ(opt_table->table, instance->table.get());
    EXPECT_EQ(opt_table->table->size(), 2);
    EXPECT_EQ((*opt_table->table)[0].instance, instance.get());
    EXPECT_EQ((*opt_table->table)[0].func_idx, 1);
    EXPECT_EQ((*opt_table->table)[1].instance, instance.get());
    EXPECT_EQ((*opt_table->table)[1].func_idx, 0);
    EXPECT_EQ(opt_table->limits.min, 2);
    ASSERT_TRUE(opt_table->limits.max.has_value());
    EXPECT_EQ(opt_table->limits.max, 20);

    EXPECT_FALSE(find_exported_table(*instance, "ttt").has_value());

    /* wat2wasm
    (module
      (table (import "test" "table") 2 20 anyfunc)
      (export "tab" (table 0))
      (func $f (export "f") nop)
      (func $g nop)
      (global (export "g1") i32 (i32.const 0))
      (memory (export "mem") 0)
    )
     */
    const auto wasm_reexported_table = from_hex(
        "0061736d010000000104016000000211010474657374057461626c650170010214030302000005030100000606"
        "017f0041000b071604037461620100016600000267310300036d656d02000a09020300010b0300010b");

    table_elements table(2);
    auto instance_reexported_table =
        instantiate(parse(wasm_reexported_table), {}, {ExternalTable{&table, {2, 20}}});

    opt_table = find_exported_table(*instance_reexported_table, "tab");
    ASSERT_TRUE(opt_table);
    EXPECT_EQ(opt_table->table, &table);
    EXPECT_EQ(opt_table->limits.min, 2);
    ASSERT_TRUE(opt_table->limits.max.has_value());
    EXPECT_EQ(opt_table->limits.max, 20);

    EXPECT_FALSE(find_exported_table(*instance, "ttt").has_value());

    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (global (export "g1") i32 (i32.const 0))
      (memory (export "mem") 0)
    )
    */
    const auto wasm_no_table = from_hex(
        "0061736d010000000104016000000302010005030100000606017f0041000b071003016600000267310300036d"
        "656d02000a05010300010b");

    auto instance_no_table = instantiate(parse(wasm_no_table));

    EXPECT_FALSE(find_exported_table(*instance_no_table, "tab").has_value());
}

TEST(api, find_exported_table_reimport)
{
    /* wat2wasm
    (module
      (table (import "test" "table") 2 20 anyfunc)
      (export "tab" (table 0))
    )
    */
    const auto wasm =
        from_hex("0061736d010000000211010474657374057461626c650170010214070701037461620100");

    // importing the table with limits narrower than defined in the module
    table_elements table(5);
    auto instance = instantiate(parse(wasm), {}, {ExternalTable{&table, {5, 10}}});

    auto opt_table = find_exported_table(*instance, "tab");
    ASSERT_TRUE(opt_table);
    EXPECT_EQ(opt_table->table, &table);
    // table should have the limits it was imported with
    EXPECT_EQ(opt_table->limits.min, 5);
    ASSERT_TRUE(opt_table->limits.max.has_value());
    EXPECT_EQ(*opt_table->limits.max, 10);

    /* wat2wasm
    (module
      (table (import "test" "table") 5 10 anyfunc)
    )
    */
    const auto wasm_reimported_table =
        from_hex("0061736d010000000211010474657374057461626c65017001050a");

    // importing the same table into the module with equal limits, instantiate should succeed
    instantiate(parse(wasm_reimported_table), {}, {*opt_table});
}

TEST(api, find_exported_memory)
{
    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000404017000000504010101020606017f0041000b07160401660000"
        "0267310300037461620100036d656d02000a05010300010b");

    auto instance = instantiate(parse(wasm));

    auto opt_memory = find_exported_memory(*instance, "mem");
    ASSERT_TRUE(opt_memory);
    EXPECT_EQ(opt_memory->data->size(), PageSize);
    EXPECT_EQ(opt_memory->limits.min, 1);
    ASSERT_TRUE(opt_memory->limits.max.has_value());
    EXPECT_EQ(opt_memory->limits.max, 2);

    EXPECT_FALSE(find_exported_memory(*instance, "mem2").has_value());

    /* wat2wasm
    (module
      (memory (import "test" "memory") 1 10)
      (export "mem" (memory 0))
      (func $f (export "f") nop)
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
    )
    */
    const auto wasm_reexported_memory = from_hex(
        "0061736d010000000104016000000211010474657374066d656d6f72790201010a030201000404017000000606"
        "017f0041000b071604036d656d02000166000002673103000374616201000a05010300010b");

    bytes memory(PageSize, 0);
    auto instance_reexported_memory =
        instantiate(parse(wasm_reexported_memory), {}, {}, {ExternalMemory{&memory, {1, 4}}});

    opt_memory = find_exported_memory(*instance_reexported_memory, "mem");
    ASSERT_TRUE(opt_memory);
    EXPECT_EQ(opt_memory->data, &memory);
    EXPECT_EQ(opt_memory->limits.min, 1);
    ASSERT_TRUE(opt_memory->limits.max.has_value());
    EXPECT_EQ(opt_memory->limits.max, 4);

    EXPECT_FALSE(find_exported_memory(*instance, "memory").has_value());

    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
    )
    */
    const auto wasm_no_memory = from_hex(
        "0061736d01000000010401600000030201000404017000000606017f0041000b07100301660000026731030003"
        "74616201000a05010300010b");

    auto instance_no_memory = instantiate(parse(wasm_no_memory));

    EXPECT_FALSE(find_exported_table(*instance_no_memory, "mem").has_value());
}

TEST(api, find_exported_memory_reimport)
{
    /* wat2wasm
    (module
      (memory (import "test" "memory") 1 10)
      (export "mem" (memory 0))
    )
    */
    const auto wasm =
        from_hex("0061736d010000000211010474657374066d656d6f72790201010a070701036d656d0200");

    // importing the memory with limits narrower than defined in the module
    bytes memory(2 * PageSize, 0);
    auto instance = instantiate(parse(wasm), {}, {}, {ExternalMemory{&memory, {2, 5}}});

    auto opt_memory = find_exported_memory(*instance, "mem");
    ASSERT_TRUE(opt_memory);
    EXPECT_EQ(opt_memory->data, &memory);
    // table should have the limits it was imported with
    EXPECT_EQ(opt_memory->limits.min, 2);
    ASSERT_TRUE(opt_memory->limits.max.has_value());
    EXPECT_EQ(*opt_memory->limits.max, 5);

    /* wat2wasm
    (module
      (memory (import "test" "memory") 2 5)
    )
    */
    const auto wasm_reimported_memory =
        from_hex("0061736d010000000211010474657374066d656d6f727902010205");

    // importing the same table into the module with equal limits, instantiate should succeed
    instantiate(parse(wasm_reimported_memory), {}, {}, {*opt_memory});
}
