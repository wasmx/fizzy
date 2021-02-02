// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(execute_control, unreachable)
{
    /* wat2wasm
    (func unreachable)
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a05010300000b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Traps());
}

TEST(execute_control, nop)
{
    /* wat2wasm
    (func nop)
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a05010300010b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result());
}

TEST(execute_control, block_br)
{
    /* wat2wasm
    (func (result i32)
        (local i32 i32)
        (block
          i32.const 0xa
          local.set 1
          br 0
          i32.const 0xb
          local.set 1
        )
        local.get 1
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f030201000a15011301027f0240410a21010c00410b21010b20010b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0xa));
}

TEST(execute_control, loop_void)
{
    /* wat2wasm
    (func (param i64 i64) (result i64)
      (loop
        local.get 0
        local.set 1
      )
      local.get 1
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001070160027e7e017e030201000a0d010b000340200021010b20010b");
    const auto result = execute(parse(wasm), 0, {1_u64, 0_u64});
    EXPECT_THAT(result, Result(1));
}

TEST(execute_control, block_void)
{
    /* wat2wasm
    (func (param i32 i32) (result i32)
      (block
        local.get 0
        local.set 1
      )
      local.get 1
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001070160027f7f017f030201000a0d010b000240200021010b20010b");
    const auto result = execute(parse(wasm), 0, {100, 99});
    EXPECT_THAT(result, Result(100));
}

TEST(execute_control, loop_void_br_if_16)
{
    /* wat2wasm
    (func (param i32) (result i32)
      (local i32)
      (loop
        local.get 0  ;; This is the input argument.
        i32.const 1
        i32.sub
        local.tee 0
        br_if 0
        local.get 0
        local.set 1
      )
      local.get 1
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000a18011601017f0340200041016b22000d00200021010b2001"
        "0b");
    EXPECT_THAT(execute(parse(wasm), 0, {16}), Result(0));
}

TEST(execute_control, loop_with_result)
{
    /* wat2wasm
    (func (result i32)
      (loop (result i32)
        i32.const 1
      )
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a09010700037f41010b0b");
    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(1));
}

TEST(execute_control, loop_with_result_br_if)
{
    /* wat2wasm
    (func (param i32) (result i32)
      (loop (result i32)
        i32.const -1
        local.get 0
        i32.const 1
        i32.sub
        local.tee 0
        br_if 0
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000a12011000037f417f200041016b22000d000b0b");
    EXPECT_THAT(execute(parse(wasm), 0, {2}), Result(-1));
}

TEST(execute_control, loop_with_result_br)
{
    /* wat2wasm
    (func (export "main") (param i32) (result i32)
      (block (result i32)
        (loop (result i32)
          i32.const 999
          local.get 0
          i32.const 1
          i32.sub
          local.tee 0
          br_if 1

          drop  ;; Stack height 0.
          br 0  ;; No stack cleaning.
        )
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f03020100070801046d61696e00000a19011700027f037f41e707200041"
        "016b22000d011a0c000b0b0b");
    EXPECT_THAT(execute(parse(wasm), 0, {3}), Result(999));
}

TEST(execute_control, loop_with_result_br_stack_cleanup)
{
    /* wat2wasm
    (func (export "main") (param i32) (result i32)
      (block (result i32)
        (loop (result i32)
          i32.const 999
          local.get 0
          i32.const 1
          i32.sub
          local.tee 0
          br_if 1

          i32.const 666  ;; Stack height 2.
          br 0           ;; Cleans stack to height 0.
        )
      )
    )    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f03020100070801046d61696e00000a1b011900027f037f41e707200041"
        "016b22000d01419a050c000b0b0b");
    EXPECT_THAT(execute(parse(wasm), 0, {3}), Result(999));
}

TEST(execute_control, blocks_without_br)
{
    /* wat2wasm
    (func (result i32)
      (local i32)
      (block
        local.get 0
        i32.const 1
        i32.add
        local.set 0
        (loop
          local.get 0
          i32.const 1
          i32.add
          local.set 0
          (block
            local.get 0
            i32.const 1
            i32.add
            local.set 0
            (loop
              local.get 0
              i32.const 1
              i32.add
              local.set 0
            )
          )
        )
      )
      local.get 0
    )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f030201000a30012e01017f0240200041016a21000340200041016a210002"
        "40200041016a21000340200041016a21000b0b0b0b20000b");

    EXPECT_THAT(execute(parse(bin), 0, {}), Result(4));
}

TEST(execute_control, nested_blocks_0)
{
    /* wat2wasm
    (func (result i32)
      (local i32)
      (block
        local.get 0
        i32.const 1
        i32.or
        local.set 0
        (block
          local.get 0
          i32.const 2
          i32.or
          local.set 0
          (block
            local.get 0
            i32.const 4
            i32.or
            local.set 0
            i32.const 1
            br 0
            local.get 0
            i32.const 8
            i32.or
            local.set 0)
          local.get 0
          i32.const 16
          i32.or
          local.set 0)
        local.get 0
        i32.const 32
        i32.or
        local.set 0)
      local.get 0
      i32.const 64
      i32.or)
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f030201000a43014101017f02402000410172210002402000410272210002"
        "402000410472210041010c00200041087221000b200041107221000b200041207221000b200041c000720b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0b1110111));
}

TEST(execute_control, nested_blocks_1)
{
    /* wat2wasm
    (func (result i32)
      (local i32)
      (block
        local.get 0
        i32.const 1
        i32.or
        local.set 0
        (block
          local.get 0
          i32.const 2
          i32.or
          local.set 0
          (block
            local.get 0
            i32.const 4
            i32.or
            local.set 0
            i32.const 1
            br 1
            local.get 0
            i32.const 8
            i32.or
            local.set 0)
          local.get 0
          i32.const 16
          i32.or
          local.set 0)
        local.get 0
        i32.const 32
        i32.or
        local.set 0)
      local.get 0
      i32.const 64
      i32.or)
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f030201000a43014101017f02402000410172210002402000410272210002"
        "402000410472210041010c01200041087221000b200041107221000b200041207221000b200041c000720b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0b1100111));
}

TEST(execute_control, nested_blocks_2)
{
    /* wat2wasm
    (func (result i32)
      (local i32)
      (block
        local.get 0
        i32.const 1
        i32.or
        local.set 0
        (block
          local.get 0
          i32.const 2
          i32.or
          local.set 0
          (block
            local.get 0
            i32.const 4
            i32.or
            local.set 0
            i32.const 1
            br 2
            local.get 0
            i32.const 8
            i32.or
            local.set 0)
          local.get 0
          i32.const 16
          i32.or
          local.set 0)
        local.get 0
        i32.const 32
        i32.or
        local.set 0)
      local.get 0
      i32.const 64
      i32.or)
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f030201000a43014101017f02402000410172210002402000410272210002"
        "402000410472210041010c02200041087221000b200041107221000b200041207221000b200041c000720b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0b1000111));
}

TEST(execute_control, nested_blocks_3)
{
    /* wat2wasm
    (func (result i32)
      (local i32)
      (block
        local.get 0
        i32.const 1
        i32.or
        local.set 0
        (block
          local.get 0
          i32.const 2
          i32.or
          local.set 0
          (block
            local.get 0
            i32.const 4
            i32.or
            local.set 0
            i32.const 1
            br 3
            local.get 0
            i32.const 8
            i32.or
            local.set 0)
          local.get 0
          i32.const 16
          i32.or
          local.set 0)
        local.get 0
        i32.const 32
        i32.or
        local.set 0)
      local.get 0
      i32.const 64
      i32.or)
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f030201000a43014101017f02402000410172210002402000410272210002"
        "402000410472210041010c03200041087221000b200041107221000b200041207221000b200041c000720b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(1));
}

TEST(execute_control, nested_br_if)
{
    /* wat2wasm
    (func (param i32) (result i32)
      (local i32)
      (block
        local.get 1
        i32.const 0x8000
        i32.or
        local.set 1
        (loop
          local.get 1
          i32.const 1
          i32.add
          local.set 1

          local.get 0
          i32.const 1
          i32.sub
          local.tee 0
          br_if 0

          local.get 1
          i32.const 0x4000
          i32.or
          local.set 1

          local.get 0
          i32.eqz
          br_if 1

          local.get 1
          i32.const 0x2000
          i32.or
          local.set 1)
        local.get 1
        i32.const 0x1000
        i32.or
        local.set 1)
      local.get 1
      i32.const 0x0800
      i32.or)
    */

    const auto bin = from_hex(
        "0061736d0100000001060160017f017f030201000a4a014801017f02402001418080027221010340200141016a"
        "2101200041016b22000d002001418080017221012000450d0120014180c0007221010b20014180207221010b20"
        "01418010720b");

    for (auto loop_count : {1u, 2u})
    {
        EXPECT_THAT(execute(parse(bin), 0, {loop_count}), Result((0b11001000 << 8) + loop_count))
            << loop_count;
    }
}

TEST(execute_control, br_stack_cleanup)
{
    /* wat2wasm
    (func (result i32)
      i32.const 1
      (block
        i32.const 2
        br 0  ;; Should drop 2 from the stack.
      )
    )
    */

    const auto bin =
        from_hex("0061736d010000000105016000017f030201000a0d010b004101024041020c000b0b");

    EXPECT_THAT(execute(parse(bin), 0, {}), Result(1));
}

TEST(execute_control, br_if_stack_cleanup)
{
    /* wat2wasm
    (func (param i32) (result i64)
      i64.const 0
      (loop
        i64.const -2  ;; Additional stack item.
        i32.const -1
        local.get 0
        i32.add
        local.tee 0
        br_if 0       ;; Clean up stack.
        drop          ;; Drop the additional stack item.
      )
    )
    */

    const auto bin = from_hex(
        "0061736d0100000001060160017f017e030201000a1501130042000340427e417f20006a22000d001a0b0b");

    EXPECT_THAT(execute(parse(bin), 0, {7}), Result(0));
}

TEST(execute_control, br_if_stack_cleanup_arity1)
{
    /* wat2wasm
    (func (param i32) (result i32)
        i64.const 1   ;; Additional stack item.
        i32.const 0
        local.get 0
        br_if 0       ;; Clean up stack.
        drop
        drop
        i32.const 1
    )
    */

    const auto bin =
        from_hex("0061736d0100000001060160017f017f030201000a10010e004201410020000d001a1a41010b");

    const auto module = parse(bin);
    EXPECT_THAT(execute(module, 0, {0}), Result(1));
    EXPECT_THAT(execute(module, 0, {1}), Result(0));
}

TEST(execute_control, br_multiple_blocks_stack_cleanup)
{
    /* wat2wasm
    (func
      (block
        i32.const 1
        (loop
          i64.const 2
          br 1
        )
        drop
      )
    )
    */

    const auto bin =
        from_hex("0061736d01000000010401600000030201000a11010f0002404101034042020c010b1a0b0b");

    EXPECT_THAT(execute(parse(bin), 0, {}), Result());
}

TEST(execute_control, block_with_result)
{
    /* wat2wasm
    (func (result i64)
      (block (result i64)
        i64.const -1
      )
    )
    */
    const auto bin = from_hex("0061736d010000000105016000017e030201000a09010700027e427f0b0b");

    EXPECT_THAT(execute(parse(bin), 0, {}), Result(uint64_t(-1)));
}

TEST(execute_control, trap_inside_block)
{
    /* wat2wasm
    (func (result i64)
      (block (result i64)
        unreachable
      )
    )
    */
    const auto bin = from_hex("0061736d010000000105016000017e030201000a08010600027e000b0b");

    EXPECT_THAT(execute(parse(bin), 0, {}), Traps());
}

TEST(execute_control, br_with_result)
{
    /* wat2wasm
    (func (result i32)
      (block (result i32)
        i32.const 1
        i32.const 2
        i32.const 3
        br 0  ;; Takes the top item as the result, drops remaining 2 items.
      )
    )
    */
    const auto bin =
        from_hex("0061736d010000000105016000017f030201000a0f010d00027f4101410241030c000b0b");

    EXPECT_THAT(execute(parse(bin), 0, {}), Result(3));
}

TEST(execute_control, br_if_with_result)
{
    /* wat2wasm
    (func (param i32) (result i32)
      (block (result i32)
        i32.const 1
        i32.const 2
        local.get 0
        br_if 0
        i32.xor
      )
    )
    */
    const auto bin =
        from_hex("0061736d0100000001060160017f017f030201000a10010e00027f4101410220000d00730b0b");

    for (const auto param : {0u, 1u})
    {
        constexpr uint32_t expected_results[]{
            3,  // br_if not taken, result: 1 xor 2 == 3.
            2,  // br_if taken, result: 2, remaining item dropped.
        };

        EXPECT_THAT(execute(parse(bin), 0, {param}), Result(expected_results[param]));
    }
}

TEST(execute_control, br_if_out_of_function)
{
    /* wat2wasm
    (func (param i32) (result i32)
      i32.const 1
      i32.const 2
      local.get 0
      br_if 0
      drop
    )
    */
    const auto bin =
        from_hex("0061736d0100000001060160017f017f030201000a0d010b004101410220000d001a0b");

    for (const auto param : {0u, 1u})
    {
        constexpr uint32_t expected_results[]{
            1,  // br_if not taken.
            2,  // br_if taken.
        };

        EXPECT_THAT(execute(parse(bin), 0, {param}), Result(expected_results[param]));
    }
}

TEST(execute_control, br_1_out_of_function_and_imported_function)
{
    /* wat2wasm
    (func (import "imported" "function"))  ;; Imported function affects code index.
    (func (result i32)
      (loop
        i32.const 1
        br 1
      )
      i32.const 0
    )
    */
    const auto bin = from_hex(
        "0061736d010000000108026000006000017f02150108696d706f727465640866756e6374696f6e000003020101"
        "0a0d010b00034041010c010b41000b");

    const auto module = parse(bin);
    auto instance = instantiate(
        *module, std::vector<ExternalFunction>(1, {ExecuteFunction{nullptr}, module->typesec[0]}));
    EXPECT_THAT(execute(*instance, 1, {}), Result(1));
}

TEST(execute_control, br_inner_nonempty_stack)
{
    /* wat2wasm
      (func (result i32)
        (i32.const 0x1)
        (block (result i32)
          (block (br 0))
          (i32.const 0x2)
        )
        i32.add
      )
     */
    const auto bin =
        from_hex("0061736d010000000105016000017f030201000a11010f004101027f02400c000b41020b6a0b");

    auto instance = instantiate(parse(bin));
    EXPECT_THAT(execute(*instance, 0, {}), Result(0x3));
}

TEST(execute_control, br_table)
{
    /* wat2wasm
    (func (param i32) (result i32)
      (block
        (block
          (block
            (block
              (block
                (br_table 3 2 1 0 4 (local.get 0))
                (return (i32.const 99))
              )
              (return (i32.const 100))
            )
            (return (i32.const 101))
          )
          (return (i32.const 102))
        )
        (return (i32.const 103))
      )
      (i32.const 104)
    )
    */
    const auto bin = from_hex(
        "0061736d0100000001060160017f017f030201000a330131000240024002400240024020000e04030201000441"
        "e3000f0b41e4000f0b41e5000f0b41e6000f0b41e7000f0b41e8000b");

    for (const auto param : {0u, 1u, 2u, 3u, 4u, 5u})
    {
        constexpr uint32_t expected_results[]{103, 102, 101, 100, 104, 104};

        EXPECT_THAT(execute(parse(bin), 0, {param}), Result(expected_results[param]));
    }

    EXPECT_THAT(execute(parse(bin), 0, {42}), Result(104));
}

TEST(execute_control, br_table_empty_vector)
{
    /* wat2wasm
    (func (param i32) (result i32)
      (block
        (br_table 0 (local.get 0))
        (return (i32.const 99))
      )
      (i32.const 100)
    )
    */
    const auto bin = from_hex(
        "0061736d0100000001060160017f017f030201000a13011100024020000e000041e3000f0b41e4000b");

    for (const auto param : {0u, 1u, 2u})
    {
        EXPECT_THAT(execute(parse(bin), 0, {param}), Result(100));
    }
}

TEST(execute_control, br_table_as_return)
{
    /* wat2wasm
    (func (param i32) (result i32)
      (block (result i32)
        i32.const 1001
        local.get 0
        br_table 0 1
      )
      drop
      i32.const 1000
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000a14011200027f41e90720000e0100010b1a41e8070b");

    for (const auto param : {0u, 1u})
    {
        EXPECT_THAT(execute(parse(wasm), 0, {param}), Result(1000 + param));
    }
}

TEST(execute_control, return_from_loop)
{
    /* wat2wasm
    (func (result i32)
      (loop
        i32.const 1
        return
        br 0
      )
      i32.const 0
    )
    */
    const auto bin =
        from_hex("0061736d010000000105016000017f030201000a0e010c00034041010f0c000b41000b");

    EXPECT_THAT(execute(parse(bin), 0, {}), Result(1));
}

TEST(execute_control, return_stack_cleanup)
{
    /* wat2wasm
    (func
      i32.const 1
      i32.const 2
      i32.const 3
      return
    )
    */
    const auto bin = from_hex("0061736d01000000010401600000030201000a0b0109004101410241030f0b");

    EXPECT_THAT(execute(parse(bin), 0, {}), Result());
}

TEST(execute_control, return_from_block_stack_cleanup)
{
    /* wat2wasm
    (func (result i32)
      (block
        i32.const -1
        i32.const 1
        return
      )
      i32.const -2
    )
    */
    const auto bin =
        from_hex("0061736d010000000105016000017f030201000a0e010c000240417f41010f0b417e0b");

    EXPECT_THAT(execute(parse(bin), 0, {}), Result(1));
}

TEST(execute_control, if_smoke)
{
    /* wat2wasm
    (func (param $x i32) (result i32)
      (local $y i32)
      local.get $x
      (if
        (then
          i32.const 4
          local.set $y
        )
      )
      local.get $y
    )
    */
    const auto bin =
        from_hex("0061736d0100000001060160017f017f030201000a11010f01017f20000440410421010b20010b");

    const auto module = parse(bin);

    for (const auto param : {0u, 1u})
    {
        constexpr uint32_t expected_results[]{
            0,  // no if branch.
            4,  // if branch.
        };

        EXPECT_THAT(execute(module, 0, {param}), Result(expected_results[param]));
    }
}

TEST(execute_control, if_else_smoke)
{
    /* wat2wasm
    (func (param $x i32) (result i32)
      local.get $x
      (if (result i32)
        (then
          i32.const 1
        )
        (else
          i32.const 2
        )
      )
    )
    */
    const auto bin =
        from_hex("0061736d0100000001060160017f017f030201000a0e010c002000047f41010541020b0b");

    const auto module = parse(bin);

    for (const auto param : {0u, 1u})
    {
        constexpr uint32_t expected_results[]{
            2,  // else branch.
            1,  // if branch.
        };

        EXPECT_THAT(execute(module, 0, {param}), Result(expected_results[param]));
    }
}


TEST(execute_control, if_return_from_branch)
{
    /* wat2wasm
    (func (param $x i32) (result i32)
      local.get $x
      (if (result i32)
        (then
          i32.const -1
          i32.const 1
          return
        )
        (else
          i32.const -2
          i32.const -1
          i32.const 2
          return
        )
      )
    )
    */
    const auto bin = from_hex(
        "0061736d0100000001060160017f017f030201000a160114002000047f417f41010f05417e417f41020f0b0b");

    const auto module = parse(bin);

    for (const auto param : {0u, 1u})
    {
        constexpr uint32_t expected_results[]{
            2,  // else branch.
            1,  // if branch.
        };

        EXPECT_THAT(execute(module, 0, {param}), Result(expected_results[param]));
    }
}

TEST(execute_control, if_br_from_branch)
{
    /* wat2wasm
    (func  (param $x i32) (result i32)
      local.get $x
      (if (result i32)
        (then
          i32.const -1
          i32.const 21
          br 0
          i32.const 5
        )
        (else
          i32.const -2
          i32.const -1
          i32.const 2
          br 0
          i32.const 5
        )
      )
    )
    */
    const auto bin = from_hex(
        "0061736d0100000001060160017f017f030201000a1c011a002000047f417f41150c00410505417e417f41020c"
        "0041050b0b");

    const auto module = parse(bin);

    for (const auto param : {0u, 1u})
    {
        constexpr uint32_t expected_results[]{
            2,   // else branch.
            21,  // if branch.
        };

        EXPECT_THAT(execute(module, 0, {param}), Result(expected_results[param]));
    }
}

TEST(execute_control, br_table_arity)
{
    /* wat2wasm
    (func  (param $x i32) (result i32)
      (block $a (result i32)
        (block $b (result i32)
          i32.const 1
          local.get $x
          br_table $a $b
        )
        drop
        i32.const 2
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000a15011300027f027f410120000e0101000b1a41020b0b");

    auto instance = parse(wasm);
    EXPECT_THAT(execute(instance, 0, {0}), Result(1));
    EXPECT_THAT(execute(instance, 0, {1}), Result(2));

    /* wat2wasm
    (func  (param $x i32) (result i32)
      (block $a
        (loop $b (result i32)
          local.get $x
          i32.const 1
          i32.sub
          local.tee $x
          br_table $a $b
        )
        drop
      )
      i32.const 2
    )
    */
    const auto wasm2 = from_hex(
        "0061736d0100000001060160017f017f030201000a180116000240037f200041016b22000e0101000b1a0b4102"
        "0b");

    EXPECT_THAT(execute(parse(wasm2), 0, {2}), Result(2));

    /* wat2wasm
    (func  (param $x i32) (result i32)
      (loop $a (result i32)
        (block $b
          local.get $x
          i32.const 1
          i32.add
          local.tee $x
          br_table $a $b
        )
        i32.const 2
        return
      )
    )
    */
    const auto wasm3 = from_hex(
        "0061736d0100000001060160017f017f030201000a18011600037f0240200041016a22000e0101000b41020f0b"
        "0b");

    EXPECT_THAT(execute(parse(wasm3), 0, {uint32_t(-1)}), Result(2));
}
