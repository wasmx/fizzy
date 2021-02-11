// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(execute_call, call)
{
    /* wat2wasm
    (func (result i32) (i32.const 0x2a002a))
    (func (result i32) (call 0))
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f03030200000a0e02070041aa80a8010b040010000b");
    EXPECT_THAT(execute(parse(wasm), 1, {}), Result(0x2a002a));
}

TEST(execute_call, call_trap)
{
    /* wat2wasm
    (func (result i32) (unreachable))
    (func (result i32) (call 0))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f03030200000a0a020300000b040010000b");

    EXPECT_THAT(execute(parse(wasm), 1, {}), Traps());
}

TEST(execute_call, call_with_arguments)
{
    /* wat2wasm
    (module
      (func $calc (param $a i32) (param $b i32) (result i32)
        local.get 1
        local.get 0
        i32.sub ;; a - b
      )
      (func (result i32)
        i32.const 13
        i32.const 17
        call $calc ;; 17 - 13 => 4
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010b0260027f7f017f6000017f03030200010a12020700200120006b0b0800410d41111000"
        "0b");

    EXPECT_THAT(execute(parse(wasm), 1, {}), Result(4));
}

TEST(execute_call, call_void_with_zero_arguments)
{
    /* wat2wasm
    (module
      (global $z (mut i32) (i32.const -1))
      (func $set
        (global.set $z (i32.const 1))
      )
      (func (result i32)
        call $set
        global.get $z
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000108026000006000017f03030200010606017f01417f0b0a0f020600410124000b06001000"
        "23000b");

    EXPECT_THAT(execute(parse(wasm), 1, {}), Result(1));
}

TEST(execute_call, call_void_with_one_argument)
{
    /* wat2wasm
    (module
      (global $z (mut i32) (i32.const -1))
      (func $set (param $a i32)
        (global.set $z (local.get $a))
      )
      (func (result i32)
        (call $set (i32.const 1))
        global.get $z
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001090260017f006000017f03030200010606017f01417f0b0a11020600200024000b080041"
        "01100023000b");

    EXPECT_THAT(execute(parse(wasm), 1, {}), Result(1));
}

TEST(execute_call, call_void_with_two_arguments)
{
    /* wat2wasm
    (module
      (global $z (mut i32) (i32.const -1))
      (func $set (param $a i32) (param $b i32)
        (global.set $z (i32.add (local.get $a) (local.get $b)))
      )
      (func (result i32)
        (call $set (i32.const 2) (i32.const 3))
        global.get $z
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010a0260027f7f006000017f03030200010606017f01417f0b0a16020900200020016a2400"
        "0b0a0041024103100023000b");

    EXPECT_THAT(execute(parse(wasm), 1, {}), Result(2 + 3));
}

TEST(execute_call, call_shared_stack_space)
{
    /* wat2wasm
    (module
      (func $double (param i32) (result i32)
        local.get 0
        local.get 0
        i32.add
      )

      (func $main (param i32) (param i32) (result i32)
        local.get 1
        local.get 0
        i32.add
        call $double
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260017f017f60027f7f017f03030200010a13020700200020006a0b0900200120006a"
        "10000b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 1, {2, 3}), Result(10));  // double(2+3)
}

TEST(execute_call, call_indirect)
{
    /* wat2wasm
      (type $out-i32 (func (result i32)))

      (table anyfunc (elem $f3 $f2 $f1 $f4 $f5))

      (func $f1 (result i32) i32.const 1)
      (func $f2 (result i32) i32.const 2)
      (func $f3 (result i32) i32.const 3)
      (func $f4 (result i64) i64.const 4)
      (func $f5 (result i32) unreachable)

      (func (param i32) (result i32)
        (call_indirect (type $out-i32) (local.get 0))
      )
    */
    const auto bin = from_hex(
        "0061736d01000000010e036000017f6000017e60017f017f03070600000001000204050170010505090b010041"
        "000b0502010003040a2106040041010b040041020b040041030b040042040b0300000b070020001100000b");

    const auto module = parse(bin);

    for (const auto param : {0u, 1u, 2u})
    {
        constexpr uint32_t expected_results[]{3, 2, 1};

        EXPECT_THAT(execute(module, 5, {param}), Result(expected_results[param]));
    }

    // immediate is incorrect type
    EXPECT_THAT(execute(module, 5, {3}), Traps());

    // called function traps
    EXPECT_THAT(execute(module, 5, {4}), Traps());

    // argument out of table bounds
    EXPECT_THAT(execute(module, 5, {5}), Traps());
}

TEST(execute_call, call_indirect_with_argument)
{
    /* wat2wasm
    (module
      (type $bin_func (func (param i32 i32) (result i32)))
      (table anyfunc (elem $f1 $f2 $f3))

      (func $f1 (param i32 i32) (result i32) (i32.div_u (local.get 0) (local.get 1)))
      (func $f2 (param i32 i32) (result i32) (i32.sub (local.get 0) (local.get 1)))
      (func $f3 (param i32) (result i32) (i32.mul (local.get 0) (local.get 0)))

      (func (param i32) (result i32)
        i32.const 31
        i32.const 7
        (call_indirect (type $bin_func) (local.get 0))
      )
    )
    */
    const auto bin = from_hex(
        "0061736d01000000010c0260027f7f017f60017f017f03050400000101040501700103030909010041000b0300"
        "01020a25040700200020016e0b0700200020016b0b0700200020006c0b0b00411f410720001100000b");

    const auto module = parse(bin);

    EXPECT_THAT(execute(module, 3, {0}), Result(31 / 7));
    EXPECT_THAT(execute(module, 3, {1}), Result(31 - 7));

    // immediate is incorrect type
    EXPECT_THAT(execute(module, 3, {2}), Traps());
}

TEST(execute_call, call_indirect_imported_table)
{
    /* wat2wasm
    (module
     (table 5 20 anyfunc)
     (export "t" (table 0))
     (elem (i32.const 0) $f3 $f2 $f1 $f4 $f5)
     (func $f1 (result i32) (i32.const 1))
     (func $f2 (result i32) (i32.const 2))
     (func $f3 (result i32) (i32.const 3))
     (func $f4 (result i64) (i64.const 4))
     (func $f5 (result i32) unreachable)
    )
    */
    const auto bin_exported_table = from_hex(
        "0061736d010000000109026000017f6000017e03060500000001000405017001051407050101740100090b0100"
        "41000b0502010003040a1905040041010b040041020b040041030b040042040b0300000b");
    auto instance_exported_table = instantiate(parse(bin_exported_table));
    auto table = find_exported_table(*instance_exported_table, "t");
    ASSERT_TRUE(table.has_value());

    /* wat2wasm
    (module
      (type $out_i32 (func (result i32)))
      (import "m" "t" (table 5 20 anyfunc))

      (func (param i32) (result i32)
        (call_indirect (type $out_i32) (local.get 0))
      )
    )
    */
    const auto bin = from_hex(
        "0061736d01000000010a026000017f60017f017f020a01016d01740170010514030201010a0901070020001100"
        "000b");

    auto instance = instantiate(parse(bin), {}, {*table});

    for (const auto param : {0u, 1u, 2u})
    {
        constexpr uint32_t expected_results[]{3, 2, 1};

        EXPECT_THAT(execute(*instance, 0, {param}), Result(expected_results[param]));
    }

    // immediate is incorrect type
    EXPECT_THAT(execute(*instance, 0, {3}), Traps());

    // called function traps
    EXPECT_THAT(execute(*instance, 0, {4}), Traps());

    // argument out of table bounds
    EXPECT_THAT(execute(*instance, 0, {5}), Traps());
}

TEST(execute_call, call_indirect_uninited_table)
{
    /* wat2wasm
      (type $out-i32 (func (result i32)))

      (table 5 anyfunc)
      (elem (i32.const 0) $f3 $f2 $f1)

      (func $f1 (result i32) i32.const 1)
      (func $f2 (result i32) i32.const 2)
      (func $f3 (result i32) i32.const 3)

      (func (param i32) (result i32)
        (call_indirect (type $out-i32) (local.get 0))
      )
    */
    const auto bin = from_hex(
        "0061736d01000000010a026000017f60017f017f030504000000010404017000050909010041000b030201000a"
        "1804040041010b040041020b040041030b070020001100000b");

    const auto module = parse(bin);

    // elements 3 and 4 are not initialized
    EXPECT_THAT(execute(module, 3, {3}), Traps());
    EXPECT_THAT(execute(module, 3, {4}), Traps());
}

TEST(execute_call, call_indirect_shared_stack_space)
{
    /* wat2wasm
    (module
      (type $ft (func (param i32) (result i32)))
      (func $double (param i32) (result i32)
        local.get 0
        local.get 0
        i32.add
      )

      (func $main (param i32) (param i32) (result i32)
        local.get 1
        local.get 0
        call_indirect (type $ft)
      )

      (table anyfunc (elem $double))
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260017f017f60027f7f017f0303020001040501700101010907010041000b01000a13"
        "020700200020006a0b0900200120001100000b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 1, {0, 10}), Result(20));  // double(10)
}

TEST(execute_call, imported_function_call)
{
    /* wat2wasm
    (import "mod" "foo" (func (result i32)))
    (func (result i32)
      call 0
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f020b01036d6f6403666f6f0000030201000a0601040010000b");

    const auto module = parse(wasm);

    constexpr auto host_foo = [](std::any&, Instance&, const Value*, int) -> ExecutionResult {
        return Value{42};
    };
    const auto& host_foo_type = module->typesec[0];

    auto instance = instantiate(*module, {{{host_foo}, host_foo_type}});

    EXPECT_THAT(execute(*instance, 1, {}), Result(42));
}

TEST(execute_call, imported_function_call_void)
{
    /* wat2wasm
    (func (import "m" "foo"))
    (func
      call 0
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000020901016d03666f6f0000030201000a0601040010000b");

    const auto module = parse(wasm);

    static bool called = false;
    constexpr auto host_foo = [](std::any&, Instance&, const Value*, int) {
        called = true;
        return Void;
    };
    const auto host_foo_type = module->typesec[0];

    auto instance = instantiate(*module, {{{host_foo}, host_foo_type}});
    execute(*instance, 1, {});
    EXPECT_TRUE(called);
}

TEST(execute_call, imported_function_call_with_arguments)
{
    /* wat2wasm
    (import "mod" "foo" (func (param i32) (result i32)))
    (func (param i32) (result i32)
      local.get 0
      call 0
      i32.const 2
      i32.add
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f020b01036d6f6403666f6f0000030201000a0b0109002000100041026a"
        "0b");

    const auto module = parse(wasm);

    constexpr auto host_foo = [](std::any&, Instance&, const Value* args, int) -> ExecutionResult {
        return Value{args[0].i32 * 2};
    };
    const auto host_foo_type = module->typesec[0];

    auto instance = instantiate(*module, {{{host_foo}, host_foo_type}});

    EXPECT_THAT(execute(*instance, 1, {20}), Result(42));
}

TEST(execute_call, imported_functions_call_indirect)
{
    /* wat2wasm
    (module
      (type $ft (func (param i32) (result i64)))
      (func $sqr    (import "env" "sqr") (param i32) (result i64))
      (func $isqrt  (import "env" "isqrt") (param i32) (result i64))
      (func $double (param i32) (result i64)
        local.get 0
        i64.extend_i32_u
        local.get 0
        i64.extend_i32_u
        i64.add
      )

      (func $main (param i32) (param i32) (result i64)
        local.get 1
        local.get 0
        call_indirect (type $ft)
      )

      (table anyfunc (elem $double $sqr $isqrt))
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260017f017e60027f7f017e02170203656e7603737172000003656e76056973717274"
        "00000303020001040501700103030909010041000b030200010a150209002000ad2000ad7c0b09002001200011"
        "00000b");

    const auto module = parse(wasm);
    ASSERT_EQ(module->typesec.size(), 2);
    ASSERT_EQ(module->importsec.size(), 2);
    ASSERT_EQ(module->codesec.size(), 2);

    constexpr auto sqr = [](std::any&, Instance&, const Value* args, int) -> ExecutionResult {
        const auto x = args[0].i32;
        return Value{uint64_t{x} * uint64_t{x}};
    };
    constexpr auto isqrt = [](std::any&, Instance&, const Value* args, int) -> ExecutionResult {
        const auto x = args[0].i32;
        return Value{(11 + uint64_t{x} / 11) / 2};
    };

    auto instance =
        instantiate(*module, {{{sqr}, module->typesec[0]}, {{isqrt}, module->typesec[0]}});
    EXPECT_THAT(execute(*instance, 3, {0, 10}), Result(20));  // double(10)
    EXPECT_THAT(execute(*instance, 3, {1, 9}), Result(81));   // sqr(9)
    EXPECT_THAT(execute(*instance, 3, {2, 50}), Result(7));   // isqrt(50)
}

TEST(execute_call, imported_function_from_another_module)
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
    const auto module1 = parse(bin1);
    auto instance1 = instantiate(*module1);

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

    auto instance2 = instantiate(parse(bin2), {*find_exported_function(*instance1, "sub")});

    EXPECT_THAT(execute(*instance2, 1, {44, 2}), Result(42));
    EXPECT_THAT(execute(*instance2, 0, {44, 2}), Result(42));
}

TEST(execute_call, imported_function_from_another_module_via_func_idx)
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
    const auto module1 = parse(bin1);
    auto instance1 = instantiate(*module1);

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

    const auto func_idx = fizzy::find_exported_function(*module1, "sub");
    ASSERT_TRUE(func_idx.has_value());

    auto instance2 = instantiate(parse(bin2), {{{*instance1, *func_idx}, module1->typesec[0]}});

    EXPECT_THAT(execute(*instance2, 1, {44, 2}), Result(42));
    EXPECT_THAT(execute(*instance2, 0, {44, 2}), Result(42));
}

TEST(execute_call, imported_function_from_another_module_via_host_function)
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
    const auto module1 = parse(bin1);
    auto instance1 = instantiate(*module1);

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

    const auto func_idx = fizzy::find_exported_function(*module1, "sub");
    ASSERT_TRUE(func_idx.has_value());

    constexpr auto sub = [](std::any& host_context, Instance&, const Value* args, int depth) {
        auto [inst1, idx] = std::any_cast<std::pair<Instance*, FuncIdx>>(host_context);
        return fizzy::execute(*inst1, idx, args, depth);
    };

    auto host_context = std::make_any<std::pair<Instance*, FuncIdx>>(instance1.get(), *func_idx);

    auto instance2 =
        instantiate(parse(bin2), {{{sub, std::move(host_context)}, module1->typesec[0]}});

    EXPECT_THAT(execute(*instance2, 1, {44, 2}), Result(42));
    EXPECT_THAT(execute(*instance2, 0, {44, 2}), Result(42));
}

TEST(execute_call, imported_table_from_another_module)
{
    /* wat2wasm
    (module
      (func $sub (param $lhs i32) (param $rhs i32) (result i32)
        local.get $lhs
        local.get $rhs
        i32.sub)
      (table (export "tab") 1 funcref)
      (elem (i32.const 0) $sub)
    )
    */
    const auto bin1 = from_hex(
        "0061736d0100000001070160027f7f017f030201000404017000010707010374616201000907010041000b0100"
        "0a09010700200020016b0b");
    auto instance1 = instantiate(parse(bin1));

    /* wat2wasm
    (module
      (type $t1 (func (param $lhs i32) (param $rhs i32) (result i32)))
      (import "m1" "tab" (table 1 funcref))

      (func $main (param i32) (param i32) (result i32)
        local.get 0
        local.get 1
        (call_indirect (type $t1) (i32.const 0))
      )
    )
    */
    const auto bin2 = from_hex(
        "0061736d0100000001070160027f7f017f020c01026d310374616201700001030201000a0d010b002000200141"
        "001100000b");

    const auto table = fizzy::find_exported_table(*instance1, "tab");
    ASSERT_TRUE(table.has_value());

    auto instance2 = instantiate(parse(bin2), {}, {*table});

    EXPECT_THAT(execute(*instance2, 0, {44, 2}), Result(42));
}

TEST(execute_call, imported_table_modified_by_uninstantiable_module)
{
    /* wat2wasm
    (module
      (type $t1 (func (param $lhs i32) (param $rhs i32) (result i32)))
      (func (param i32) (param i32) (result i32)
        local.get 0
        local.get 1
        (call_indirect (type $t1) (i32.const 0))
      )
      (table (export "tab") 1 funcref)
    )
    */
    const auto bin1 = from_hex(
        "0061736d0100000001070160027f7f017f030201000404017000010707010374616201000a0d010b0020002001"
        "41001100000b");
    auto instance1 = instantiate(parse(bin1));

    /* wat2wasm
    (module
      (import "m1" "tab" (table 1 funcref))
      (func $sub (param $lhs i32) (param $rhs i32) (result i32)
        local.get $lhs
        local.get $rhs
        i32.sub)
      (elem (i32.const 0) $sub)
      (func $main (unreachable))
      (start $main)
    )
    */
    const auto bin2 = from_hex(
        "0061736d01000000010a0260027f7f017f600000020c01026d3103746162017000010303020001080101090701"
        "0041000b01000a0d020700200020016b0b0300000b");

    const auto table = fizzy::find_exported_table(*instance1, "tab");
    ASSERT_TRUE(table.has_value());

    EXPECT_THROW_MESSAGE(instantiate(parse(bin2), {}, {*table}), instantiate_error,
        "start function failed to execute");

    EXPECT_THAT(execute(*instance1, 0, {44, 2}), Result(42));
}

TEST(execute_call, call_infinite_recursion)
{
    /* wat2wasm
    (module (func call 0))
    */
    const auto bin = from_hex("0061736d01000000010401600000030201000a0601040010000b");

    const auto module = parse(bin);

    EXPECT_THAT(execute(module, 0, {}), Traps());
}

TEST(execute_call, call_indirect_infinite_recursion)
{
    /* wat2wasm
      (type $out-i32 (func (result i32)))
      (table anyfunc (elem $foo))
      (func $foo (result i32)
        (call_indirect (type $out-i32) (i32.const 0))
      )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f03020100040501700101010907010041000b01000a090107004100110000"
        "0b");

    const auto module = parse(bin);

    EXPECT_TRUE(execute(module, 0, {}).trapped);
}

constexpr auto TestCallStackLimit = 2048;
static_assert(TestCallStackLimit == CallStackLimit);

TEST(execute_call, call_initial_depth)
{
    /* wat2wasm
    (import "mod" "foo" (func))
    */

    const auto wasm = from_hex("0061736d01000000010401600000020b01036d6f6403666f6f0000");

    auto module = parse(wasm);
    constexpr auto host_foo = [](std::any&, Instance&, const Value*, int depth) {
        EXPECT_EQ(depth, 0);
        return Void;
    };
    const auto host_foo_type = module->typesec[0];

    auto instance = instantiate(std::move(module), {{{host_foo}, host_foo_type}});

    EXPECT_THAT(execute(*instance, 0, {}), Result());
}

TEST(execute_call, call_max_depth)
{
    /* wat2wasm
    (func (result i32) (i32.const 42))
    (func (result i32) (call 0))
    */

    const auto bin = from_hex("0061736d010000000105016000017f03030200000a0b020400412a0b040010000b");

    auto instance = instantiate(parse(bin));

    EXPECT_THAT(execute(*instance, 0, {}, TestCallStackLimit - 1), Result(42));
    EXPECT_THAT(execute(*instance, 1, {}, TestCallStackLimit - 1), Traps());
}

TEST(execute_call, execute_imported_max_depth)
{
    /* wat2wasm
    (import "mod" "foo" (func))
    (func)
    */

    const auto wasm =
        from_hex("0061736d01000000010401600000020b01036d6f6403666f6f0000030201000a040102000b");

    auto module = parse(wasm);
    constexpr auto host_foo = [](std::any&, Instance&, const Value*, int depth) {
        EXPECT_LE(depth, TestCallStackLimit - 1);
        return Void;
    };
    const auto host_foo_type = module->typesec[0];

    auto instance = instantiate(std::move(module), {{{host_foo}, host_foo_type}});

    EXPECT_THAT(execute(*instance, 0, {}, TestCallStackLimit - 1), Result());
    EXPECT_THAT(execute(*instance, 1, {}, TestCallStackLimit - 1), Result());
    EXPECT_THAT(execute(*instance, 0, {}, TestCallStackLimit), Traps());
    EXPECT_THAT(execute(*instance, 1, {}, TestCallStackLimit), Traps());
}

TEST(execute_call, imported_function_from_another_module_max_depth)
{
    /* wat2wasm
    (module
      (func)
      (export "f" (func 0))
    )
    */
    const auto bin1 = from_hex("0061736d0100000001040160000003020100070501016600000a040102000b");
    auto module1 = parse(bin1);
    auto instance1 = instantiate(std::move(module1));

    /* wat2wasm
    (module
      (func (import "m1" "f"))
      (func)
      (func call 0)
      (func call 1)
    )
    */
    const auto bin2 = from_hex(
        "0061736d01000000010401600000020801026d31016600000304030000000a0e0302000b040010000b04001001"
        "0b");
    auto module2 = parse(bin2);

    const auto func_idx = fizzy::find_exported_function(*instance1->module, "f");
    ASSERT_TRUE(func_idx.has_value());

    constexpr auto sub = [](std::any& host_context, Instance&, const Value* args, int depth) {
        auto [inst1, idx] = std::any_cast<std::pair<Instance*, FuncIdx>>(host_context);
        return fizzy::execute(*inst1, idx, args, depth + 1);
    };

    auto host_context = std::make_any<std::pair<Instance*, FuncIdx>>(instance1.get(), *func_idx);

    auto instance2 = instantiate(
        std::move(module2), {{{sub, std::move(host_context)}, instance1->module->typesec[0]}});

    EXPECT_THAT(execute(*instance2, 2, {}, TestCallStackLimit - 1 - 1), Traps());
    EXPECT_THAT(execute(*instance2, 3, {}, TestCallStackLimit - 1 - 1), Result());
}

TEST(execute_call, count_calls_to_imported_function)
{
    /* wat2wasm
    (global $counter (import "m0" "counter") (mut i64))
    (func $infinite (export "infinite")
      (global.set $counter (i64.add (global.get $counter) (i64.const 1)))
      (call $infinite)
    )
    */
    const auto wasm1 = from_hex(
        "0061736d01000000010401600000020f01026d3007636f756e746572037e0103020100070c0108696e66696e69"
        "746500000a0d010b00230042017c240010000b");

    Value counter;

    auto i1 = instantiate(parse(wasm1), {}, {}, {}, {{&counter, {ValType::i64, true}}});
    counter.i64 = 0;
    EXPECT_THAT(execute(*i1, 0, {}), Traps());
    EXPECT_EQ(counter.i64, TestCallStackLimit);


    /* wat2wasm
    (func (import "m1" "infinite"))
    */
    const auto wasm2 = from_hex("0061736d01000000010401600000020f01026d3108696e66696e6974650000");
    auto i2 = instantiate(parse(wasm2), {*find_exported_function(*i1, "infinite")});
    counter.i64 = 0;
    EXPECT_THAT(execute(*i2, 0, {}), Traps());
    EXPECT_EQ(counter.i64, TestCallStackLimit);
}

// A regression test for incorrect number of arguments passed to a call.
TEST(execute_call, call_nonempty_stack)
{
    /* wat2wasm
    (func (param i32) (result i32)
      local.get 0
    )
    (func (result i32)
      i32.const 1
      i32.const 2
      call 0
      i32.add
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010a0260017f017f6000017f03030200010a1002040020000b09004101410210006a0b");

    auto instance = instantiate(parse(wasm));

    EXPECT_THAT(execute(*instance, 1, {}), Result(3));
}

TEST(execute_call, call_imported_infinite_recursion)
{
    /* wat2wasm
    (import "mod" "foo" (func (result i32)))
    (func $f (result i32)
      call 0
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f020b01036d6f6403666f6f0000030201000a0601040010000b");

    const auto module = parse(wasm);
    static int counter = 0;
    constexpr auto host_foo = [](std::any&, Instance& instance, const Value* args, int depth) {
        ++counter;
        return execute(instance, 0, args, depth + 1);
    };
    const auto host_foo_type = module->typesec[0];

    auto instance = instantiate(*module, {{{host_foo}, host_foo_type}});

    counter = 0;
    EXPECT_THAT(execute(*instance, 0, {}), Traps());
    EXPECT_EQ(counter, TestCallStackLimit);

    counter = 0;
    EXPECT_THAT(execute(*instance, 1, {}), Traps());
    EXPECT_EQ(counter, TestCallStackLimit - 1);
}

TEST(execute_call, call_imported_interleaved_infinite_recursion)
{
    /* wat2wasm
    (import "mod" "foo" (func (result i32)))
    (func $f (result i32)
      call 0
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f020b01036d6f6403666f6f0000030201000a0601040010000b");

    const auto module = parse(wasm);
    static int counter = 0;
    constexpr auto host_foo = [](std::any&, Instance& instance, const Value* args, int depth) {
        // Function $f will increase depth. This means each iteration goes 2 steps deeper.
        EXPECT_LT(depth, CallStackLimit);
        ++counter;
        return execute(instance, 1, args, depth + 1);
    };
    const auto host_foo_type = module->typesec[0];

    auto instance = instantiate(*module, {{{host_foo}, host_foo_type}});

    // Start with the imported host function.
    counter = 0;
    EXPECT_THAT(execute(*instance, 0, {}), Traps());
    EXPECT_EQ(counter, CallStackLimit / 2);

    // Start with the wasm function.
    counter = 0;
    EXPECT_THAT(execute(*instance, 1, {}), Traps());
    EXPECT_EQ(counter, CallStackLimit / 2);
}

TEST(execute_call, call_imported_max_depth_recursion)
{
    /* wat2wasm
    (import "mod" "foo" (func (result i32)))
    */
    const auto wasm = from_hex("0061736d010000000105016000017f020b01036d6f6403666f6f0000");

    const auto module = parse(wasm);
    constexpr auto host_foo = [](std::any&, Instance& instance, const Value* args,
                                  int depth) -> ExecutionResult {
        if (depth == TestCallStackLimit - 1)
            return Value{uint32_t{1}};  // Terminate recursion on the max depth.
        return execute(instance, 0, args, depth + 1);
    };
    const auto host_foo_type = module->typesec[0];

    auto instance = instantiate(*module, {{{host_foo}, host_foo_type}});

    EXPECT_THAT(execute(*instance, 0, {}), Result(uint32_t{1}));
}

TEST(execute_call, call_via_imported_max_depth_recursion)
{
    /* wat2wasm
    (import "mod" "foo" (func (result i32)))
    (func $f (result i32)
      call 0
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f020b01036d6f6403666f6f0000030201000a0601040010000b");

    const auto module = parse(wasm);
    auto host_foo = [](std::any&, Instance& instance, const Value* args,
                        int depth) -> ExecutionResult {
        // Function $f will increase depth. This means each iteration goes 2 steps deeper.
        if (depth == TestCallStackLimit - 1)
            return Value{uint32_t{1}};  // Terminate recursion on the max depth.
        return execute(instance, 1, args, depth + 1);
    };
    const auto host_foo_type = module->typesec[0];

    auto instance = instantiate(*module, {{{host_foo}, host_foo_type}});

    EXPECT_THAT(execute(*instance, 1, {}), Result(uint32_t{1}));
}

TEST(execute_call, call_indirect_imported_table_infinite_recursion)
{
    /* wat2wasm
    (module
      (type (func (result i32)))
      (table (export "tab") 2 funcref)
      (elem (i32.const 0) $f1)
      (func $f1 (result i32)
        (call_indirect (type 0) (i32.const 1))
      )
    )
    */
    const auto bin1 = from_hex(
        "0061736d010000000105016000017f030201000404017000020707010374616201000907010041000b01000a09"
        "01070041011100000b");
    auto instance1 = instantiate(parse(bin1));

    /* wat2wasm
    (module
      (type (func (result i32)))
      (import "m1" "tab" (table 1 funcref))
      (elem (i32.const 1) $f2)
      (func $f2 (result i32)
        (call_indirect (type 0) (i32.const 0))
      )
    )
    */
    const auto bin2 = from_hex(
        "0061736d010000000105016000017f020c01026d310374616201700001030201000907010041010b01000a0901"
        "070041001100000b");

    const auto table = fizzy::find_exported_table(*instance1, "tab");
    ASSERT_TRUE(table.has_value());

    auto instance2 = instantiate(parse(bin2), {}, {*table});

    EXPECT_THAT(execute(*instance1, 0, {}), Traps());
}

TEST(execute_call, drop_call_result)
{
    // Regression test for incorrect max_stack_height based on call.wast:287.
    /* wat2wasm
      (func $const-i32 (result i32) (i32.const 0x132))
      (func (export "drop_call_result")
        call $const-i32
        drop
      )
    */
    const auto wasm = from_hex(
        "0061736d010000000108026000017f60000003030200010714011064726f705f63616c6c5f726573756c740001"
        "0a0d02050041b2020b050010001a0b");

    const auto module = parse(wasm);
    ASSERT_EQ(module->codesec.size(), 2);
    EXPECT_EQ(module->codesec[0].max_stack_height, 1);
    EXPECT_EQ(module->codesec[1].max_stack_height, 1);
    const auto func_idx = find_exported_function(*module, "drop_call_result");
    auto instance = instantiate(*module);
    EXPECT_THAT(fizzy::execute(*instance, *func_idx, {}), Result());
}
