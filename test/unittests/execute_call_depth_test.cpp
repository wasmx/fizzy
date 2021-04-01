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
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Result(1_u32));
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit), Traps());
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
    EXPECT_THAT(execute(*executor, 0, {}, DepthLimit - 1), Result(1_u32));
    EXPECT_THAT(execute(*executor, 0, {}, DepthLimit), Traps());
}

TEST(execute_call_depth, execute_imported_host_function)
{
    /* wat2wasm
    (func (import "host" "f") (result i32))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020a0104686f737401660000");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance&, const Value*, ExecutionContext& ctx) noexcept {
        recorded_depth = ctx.depth;
        return ExecutionResult{Value{1}};
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 0);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}

TEST(execute_call_depth, execute_imported_host_function_calling_wasm_function)
{
    // The imported host function $host_f is executed first. It then calls $leaf internal wasm
    // function. The host function does not bump the call depth.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func $leaf (result i32) (i32.const 1))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020a0104686f737401660000030201000a0601040041010b");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance& instance, const Value*,
                                ExecutionContext& ctx) noexcept {
        recorded_depth = ctx.depth;
        return fizzy::execute(instance, 1, {}, ctx);
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 0);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 2);

    // Host function is not included in the depth limit, so 1 slot is enough.
    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit), Traps());
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
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit), Traps());
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
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 1), Traps());
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit), Traps());
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
    constexpr auto host_f = [](std::any&, Instance&, const Value*, ExecutionContext& ctx) noexcept {
        recorded_depth = ctx.depth;
        return ExecutionResult{Value{1}};
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(recorded_depth, -1000);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}

TEST(execute_call_depth, call_host_function_calling_wasm_function_inclusive)
{
    // Test for "wasm-host-wasm" sandwich.
    // The host function bumps depth and pass it along.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func (result i32) (call $host_f))
    (func $leaf (result i32) (i32.const 1))
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f020a0104686f73740166000003030200000a0b02040010000b040041010"
        "b");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance& instance, const Value*,
                                ExecutionContext& ctx) noexcept {
        recorded_depth = ctx.depth;
        const auto local_ctx = ctx.create_local_context();
        return fizzy::execute(instance, 2 /* $leaf */, {}, ctx);
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 3), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 2);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Traps());
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(recorded_depth, -1000);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}

TEST(execute_call_depth, call_host_function_calling_wasm_function_exclusive)
{
    // Test for "wasm-host-wasm" sandwich.
    // The host function only passes the depth along without bumping it.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func (result i32) (call $host_f))
    (func $leaf (result i32) (i32.const 1))
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f020a0104686f73740166000003030200000a0b02040010000b040041010"
        "b");

    static int recorded_depth;
    constexpr auto host_f = [](std::any&, Instance& instance, const Value*,
                                ExecutionContext& ctx) noexcept {
        recorded_depth = ctx.depth;
        return fizzy::execute(instance, 2, {}, ctx);
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(recorded_depth, -1000);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}

TEST(execute_call_depth, call_host_function_calling_another_wasm_module)
{
    // Test for "wasm-host-wasm" sandwich.
    // The host function is obligated to bump depth and pass it along.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func (result i32) (call $host_f))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020a0104686f737401660000030201000a0601040010000b");

    /* wat2wasm
    (func (result i32) (i32.const 1))
    */
    const auto another_wasm = from_hex("0061736d010000000105016000017f030201000a0601040041010b");

    static int recorded_depth;
    constexpr auto host_f = [](std::any& host_context, Instance&, const Value*,
                                ExecutionContext& ctx) noexcept {
        recorded_depth = ctx.depth;
        auto instance = *std::any_cast<Instance*>(&host_context);
        const auto local_ctx = ctx.create_local_context();
        return fizzy::execute(*instance, 0, {}, ctx);
    };

    auto another_instance = instantiate(parse(another_wasm));

    auto host_context = std::make_any<Instance*>(another_instance.get());

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f, host_context}, module->typesec[0]}});

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}), Result(1_u32));
    EXPECT_EQ(recorded_depth, 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 3), Result(1_u32));
    EXPECT_EQ(recorded_depth, DepthLimit - 2);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 2), Traps());
    EXPECT_EQ(recorded_depth, DepthLimit - 1);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(recorded_depth, -1000);

    recorded_depth = -1000;
    EXPECT_THAT(execute(*instance, 1, {}, DepthLimit), Traps());
    EXPECT_EQ(recorded_depth, -1000);
}


/// Infinite recursion

TEST(execute_call_depth, execute_internal_function_infinite_recursion)
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

    // When starting from depth 0, the $f is expected to be called DepthLimit times.
    counter.i64 = 0;
    EXPECT_THAT(execute(*instance, 0, {}), Traps());
    EXPECT_EQ(counter.i64, DepthLimit);

    // Here only single depth level is available, so $f is called once.
    counter.i64 = 0;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(counter.i64, 1);

    // Here execution traps immediately, the $f is never called.
    counter.i64 = 0;
    EXPECT_THAT(execute(*instance, 0, {}, DepthLimit), Traps());
    EXPECT_EQ(counter.i64, 0);
}

TEST(execute_call_depth, execute_start_function_infinite_recursion)
{
    // This execution must always trap.
    // Number of $start invocations is counted in the imported global $counter.

    /* wat2wasm
    (global $counter (import "host" "counter") (mut i64))
    (func $start
      (global.set $counter (i64.add (global.get $counter) (i64.const 1)))
      (call $start)
    )
    (start $start)
    */
    const auto wasm = from_hex(
        "0061736d0100000001040160000002110104686f737407636f756e746572037e01030201000801000a0d010b00"
        "230042017c240010000b");

    Value counter;
    counter.i64 = 0;
    EXPECT_THROW_MESSAGE(instantiate(parse(wasm), {}, {}, {}, {{&counter, {ValType::i64, true}}}),
        instantiate_error, "start function failed to execute");

    // the $start is expected to be called DepthLimit times.
    EXPECT_EQ(counter.i64, DepthLimit);
}

TEST(execute_call_depth, execute_imported_wasm_function_infinite_recursion)
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
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 2), Traps());
    EXPECT_EQ(counter.i64, 1);

    // Here the only depth level available is used on the executor's main function
    // and execution traps before $f is called.
    counter.i64 = 0;
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit - 1), Traps());
    EXPECT_EQ(counter.i64, 0);

    // Here execution traps immediately, the $f is never called.
    counter.i64 = 0;
    EXPECT_THAT(execute(*executor, 1, {}, DepthLimit), Traps());
    EXPECT_EQ(counter.i64, 0);
}

TEST(execute_call_depth, execute_host_function_within_wasm_recursion_limit)
{
    // In this test the host_f host function uses wasm call depth limit
    // to protect itself against infinite recursion.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020a0104686f737401660000");

    static int max_recorded_wasm_recursion_depth;

    constexpr auto host_f = [](std::any&, Instance& instance, const Value*,
                                ExecutionContext& ctx) noexcept {
        max_recorded_wasm_recursion_depth = std::max(max_recorded_wasm_recursion_depth, ctx.depth);
        const auto local_ctx = ctx.create_local_context();
        return fizzy::execute(instance, 0, {}, ctx);
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    EXPECT_EQ(max_recorded_wasm_recursion_depth, 0);
    EXPECT_THAT(execute(*instance, 0, {}), Traps());
    EXPECT_EQ(max_recorded_wasm_recursion_depth, DepthLimit - 1);
}

TEST(execute_call_depth, execute_host_function_with_custom_recursion_limit)
{
    // In this test the host_f host function implements independent recursion depth limit.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020a0104686f737401660000");

    static constexpr int host_recursion_limit = 10;
    static int host_recursion_depth;
    static int max_recorded_wasm_recursion_depth;
    static int max_recorded_host_recursion_depth;

    constexpr auto host_f = [](std::any&, Instance& instance, const Value*,
                                ExecutionContext& ctx) noexcept {
        ++host_recursion_depth;

        assert(ctx.depth == 0);
        max_recorded_wasm_recursion_depth = std::max(max_recorded_wasm_recursion_depth, ctx.depth);
        max_recorded_host_recursion_depth =
            std::max(max_recorded_host_recursion_depth, host_recursion_depth);

        const auto result = (host_recursion_depth < host_recursion_limit) ?
                                fizzy::execute(instance, 0, {}, ctx) :
                                ExecutionResult{Value{1}};
        --host_recursion_depth;
        return result;
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    EXPECT_EQ(host_recursion_depth, 0);
    EXPECT_EQ(max_recorded_host_recursion_depth, 0);
    EXPECT_EQ(max_recorded_wasm_recursion_depth, 0);
    EXPECT_THAT(execute(*instance, 0, {}), Result(1_u32));
    EXPECT_EQ(max_recorded_wasm_recursion_depth, 0);
    EXPECT_EQ(max_recorded_host_recursion_depth, host_recursion_limit);
    EXPECT_EQ(host_recursion_depth, 0);
}

TEST(execute_call, call_host_function_calling_wasm_interleaved_infinite_recursion_inclusive)
{
    // In this test the host function host_f bumps the wasm call depth
    // including itself in the call depth limit.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func (result i32) (call $host_f))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020a0104686f737401660000030201000a0601040010000b");

    static int counter = 0;
    constexpr auto host_f = [](std::any&, Instance& instance, const Value*,
                                ExecutionContext& ctx) noexcept {
        EXPECT_LT(ctx.depth, DepthLimit);
        ++counter;
        const auto local_ctx = ctx.create_local_context();
        return fizzy::execute(instance, 1, {}, ctx);
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    // Start with the imported host function.
    counter = 0;
    EXPECT_THAT(execute(*instance, 0, {}), Traps());
    EXPECT_EQ(counter, DepthLimit / 2);

    // Start with the internal wasm function.
    counter = 0;
    EXPECT_THAT(execute(*instance, 1, {}), Traps());
    EXPECT_EQ(counter, DepthLimit / 2);
}

TEST(execute_call, call_host_function_calling_wasm_interleaved_infinite_recursion_exclusive)
{
    // In this test the host function host_f only passes the wasm call depth along
    // excluding itself in the call depth limit.

    /* wat2wasm
    (func $host_f (import "host" "f") (result i32))
    (func (result i32) (call $host_f))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f020a0104686f737401660000030201000a0601040010000b");

    static int counter = 0;
    constexpr auto host_f = [](std::any&, Instance& instance, const Value*,
                                ExecutionContext& ctx) noexcept {
        EXPECT_LT(ctx.depth, DepthLimit);
        ++counter;
        return fizzy::execute(instance, 1, {}, ctx);
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_f}, module->typesec[0]}});

    // Warning! Going up to the wasm call depth limit with host functions not counted in
    // causes OS stack overflow when build with GCC's ASan. Therefore the test starts at the depth
    // being the 1/2 of the limit.
    constexpr auto start_depth = DepthLimit / 2;

    // Start with the imported host function.
    // Wasm and host functions are executed the same number of times.
    // host, wasm, ... , host, wasm, host, wasm:TRAP.
    counter = 0;
    EXPECT_THAT(execute(*instance, 0, {}, start_depth), Traps());
    EXPECT_EQ(counter, DepthLimit - start_depth);

    // Start with the internal wasm function.
    // Host function is execute one time less than the wasm function.
    // wasm, host, ... , wasm, host, wasm:TRAP.
    counter = 0;
    EXPECT_THAT(execute(*instance, 1, {}, start_depth), Traps());
    EXPECT_EQ(counter, DepthLimit - start_depth - 1);
}
