// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>

/// @file
/// A set of unit tests inspecting the behavior of call depth limiting.

using namespace fizzy;
using namespace fizzy::test;

/// Executing at DepthLimit call stack depth immediately traps.
/// E.g. to create "space" for n calls use `DepthLimit - n`.
constexpr auto DepthLimit = 2048;
static_assert(DepthLimit == CallStackLimit);

TEST(execute_call_depth, execute_internal_function)
{
    /* wat2wasm
    (func (result i32) (i32.const 1))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0601040041010b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Traps());
}

TEST(execute_call_depth, execute_imported_wasm_function)
{
    /* wat2wasm
    (func (export "f") (result i32) (i32.const 1))
    */
    const auto exported_wasm =
        from_hex("0061736d010000000105016000017f03020100070501016600000a0601040041010b");

    /* wat2wasm
    (func (import "exporter" "f") (result i32))
    */
    const auto executor_wasm =
        from_hex("0061736d010000000105016000017f020e01086578706f7274657201660000");

    auto exporter = instantiate(parse(exported_wasm));
    auto executor = instantiate(parse(executor_wasm), {*find_exported_function(*exporter, "f")});
    EXPECT_THAT(execute(*executor, 0, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_THAT(execute(*executor, 0, {}, DepthLimit - 1), Traps());
}

TEST(execute_call_depth, execute_imported_host_function)
{
    /* wat2wasm
    (func (import "host" "f") (result i32))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020a0104686f737401660000");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance&, const Value*, int depth) noexcept {
        recorded_depth = depth;
        return ExecutionResult{Value{1_u32}};
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 0);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}

TEST(execute_call_depth, execute_imported_host_function_calling_wasm_function)
{
    // The imported host function $host_f is executed first. It then calls $leaf internal wasm
    // function. The host function is obligated to bump depth and pass it along.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func $leaf (result i32) (i32.const 1))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020a0104686f737401660000030201000a0601040041010b");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance& instance, const Value*, int depth) noexcept {
        recorded_depth = depth;
        return fizzy::execute(instance, 1, {}, depth);
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 0);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 3), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 2);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 2), Traps());
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}


TEST(execute_call_depth, call_internal_function)
{
    /* wat2wasm
    (func $internal (result i32) (i32.const 1))
    (func (result i32) (call $internal))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f03030200000a0b02040041010b040010000b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 3), Result(1_u32));
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Traps());
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
}

TEST(execute_call_depth, call_imported_wasm_function)
{
    /* wat2wasm
    (func (export "f") (result i32) (i32.const 1))
    */
    const auto exported_wasm =
        from_hex("0061736d010000000105016000017f03020100070501016600000a0601040041010b");

    /* wat2wasm
    (func $exporter_f (import "exporter" "f") (result i32))
    (func (result i32) (call $exporter_f))
    */
    const auto executor_wasm = from_hex(
        "0061736d010000000105016000017f020e01086578706f7274657201660000030201000a0601040010000b");

    auto exporter = instantiate(parse(exported_wasm));
    auto executor = instantiate(parse(executor_wasm), {*find_exported_function(*exporter, "f")});
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 3), Result(1_u32));
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 2), Traps());
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 1), Traps());
}

TEST(execute_call_depth, call_imported_host_function)
{
    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func (result i32) (call $host_f))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020a0104686f737401660000030201000a0601040010000b");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance&, const Value*, int depth) noexcept {
        recorded_depth = depth;
        return ExecutionResult{Value{1_u32}};
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 3), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Traps());
    EXPECT_EQ(recorded_depth, -1000);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}

TEST(execute_call_depth, call_host_function_calling_wasm_function)
{
    // Test for "wasm-host-wasm" sandwich.
    // The host function is obligated to bump depth and pass it along.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func (result i32) (call $host_f))
    (func $leaf (result i32) (i32.const 1))
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f020a0104686f73740166000003030200000a0b02040010000b040041010"
        "b");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance& instance, const Value*, int depth) noexcept {
        recorded_depth = depth;
        return fizzy::execute(instance, 2, {}, depth);
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 4), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 2);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 3), Traps());
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Traps());
    EXPECT_EQ(recorded_depth, -1000);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}


/// Infinite recursion

TEST(execute_call_depth, execute_internal_infinite_recursion_function)
{
    // This execution must always trap.
    // Number of $f invocations is counted in the imported global $counter.

    /* wat2wasm
    (global $counter (import "host" "counter") (mut i64))
    (func $f
      (global.set $counter (i64.add (global.get $counter) (i64.const 1)))
      (call $f)
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001040160000002110104686f737407636f756e746572037e01030201000a0d010b00230042"
        "017c240010000b");

    Value counter;
    auto instance = instantiate(parse(wasm), {}, {}, {}, {{&counter, {ValType::i64, true}}});

    // When starting from depth 0, the $f is expected to be called CallStackLimit times.
    counter.i64 = 0;
    EXPECT_THAT(execute(*instance, 0, {}), Traps());
    EXPECT_EQ(counter.i64, DepthLimit);

    // Here only single depth level is available, so $f is called once.
    counter.i64 = 0;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 2), Traps());
    EXPECT_EQ(counter.i64, 1);

    // Here execution traps immediately, the $f is never called.
    counter.i64 = 0;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(counter.i64, 0);
}

TEST(execute_call_depth, execute_imported_wasm_infinite_recursion_function)
{
    // This execution must always trap.
    // Number of $f invocations is counted in the imported global $counter.

    /* wat2wasm
    (global $counter (import "host" "counter") (mut i64))
    (func $f (export "f")
      (global.set $counter (i64.add (global.get $counter) (i64.const 1)))
      (call $f)
    )
    */
    const auto exported_wasm = from_hex(
        "0061736d0100000001040160000002110104686f737407636f756e746572037e0103020100070501016600000a"
        "0d010b00230042017c240010000b");

    /* wat2wasm
    (func $exporter_f (import "exporter" "f"))
    (func (call $exporter_f))
    */
    const auto executor_wasm = from_hex(
        "0061736d01000000010401600000020e01086578706f7274657201660000030201000a0601040010000b");

    Value counter;
    auto exporter =
        instantiate(parse(exported_wasm), {}, {}, {}, {{&counter, {ValType::i64, true}}});
    auto executor = instantiate(parse(executor_wasm), {*find_exported_function(*exporter, "f")});

    // When starting from depth 0, the $f is expected to be called CallStackLimit-1 times.
    counter.i64 = 0;
    EXPECT_THAT(execute(*executor, 1, {}), Traps());
    EXPECT_EQ(counter.i64, DepthLimit - 1);

    // Here two depth levels are available: one is used for executor's main function,
    // second is used for $f (the $f is called once).
    counter.i64 = 0;
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 3), Traps());
    EXPECT_EQ(counter.i64, 1);

    // Here the only depth level available is used on the executor's main function
    // and execution traps before $f is called.
    counter.i64 = 0;
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 2), Traps());
    EXPECT_EQ(counter.i64, 0);

    // Here execution traps immediately, the $f is never called.
    counter.i64 = 0;
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(counter.i64, 0);
}
