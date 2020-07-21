// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(validation_stack_type, instruction_type_mismatch)
{
    /* wat2wasm --no-check
    (func (result i32)
      i32.const 0
      i64.const 0
      i32.add
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a09010700410042006a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "type mismatch");
}

TEST(validation_stack_type, instruction_multiple_args)
{
    /* wat2wasm
    (memory 1)
    (func
      i32.const 0
      i64.const 0
      i64.store
    )
    */
    const auto wasm =
        from_hex("0061736d010000000104016000000302010005030100010a0b010900410042003703000b");
    EXPECT_NO_THROW(parse(wasm));

    /* wat2wasm --no-check
    (memory 1)
    (func
      i64.const 0
      i32.const 0
      i64.store
    )
    */
    const auto wasm_invalid =
        from_hex("0061736d010000000104016000000302010005030100010a0b010900420041003703000b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid), validation_error, "type mismatch");
}

TEST(validation_stack_type, unreachable_instruction)
{
    /* wat2wasm
    (func (result i32)
      unreachable
      i32.add
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a06010400006a0b");
    EXPECT_NO_THROW(parse(wasm));

    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 0
      i32.add
    )
    */
    const auto wasm_arg = from_hex("0061736d010000000105016000017f030201000a080106000041006a0b");
    EXPECT_NO_THROW(parse(wasm_arg));

    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 0
      i32.const 0
      i32.add
    )
    */
    const auto wasm_multi_arg =
        from_hex("0061736d010000000105016000017f030201000a0a01080000410041006a0b");
    EXPECT_NO_THROW(parse(wasm_multi_arg));

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      i64.const 0
      i32.const 0
      i32.add
    )
    */
    const auto wasm_invalid =
        from_hex("0061736d010000000105016000017f030201000a0a01080000420041006a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid), validation_error, "type mismatch");
}

TEST(validation_stack_type, call_multiple_args)
{
    /* wat2wasm
    (func (param i32 i64))
    (func
      i32.const 0
      i64.const 0
      call 0
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001090260027f7e0060000003030200010a0d0202000b08004100420010000b");
    EXPECT_NO_THROW(parse(wasm));

    /* wat2wasm --no-check
    (func (param i32 i64))
    (func
      i64.const 0
      i32.const 0
      call 0
    )
    */
    const auto wasm_invalid =
        from_hex("0061736d0100000001090260027f7e0060000003030200010a0d0202000b08004200410010000b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid), validation_error, "type mismatch");
}

TEST(validation_stack_type, unreachable_call)
{
    /* wat2wasm
    (func (param i32))
    (func
      unreachable
      call 0
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001080260017f0060000003030200010a0a0202000b05000010000b");
    EXPECT_NO_THROW(parse(wasm));

    /* wat2wasm
    (func (param i32 i32))
    (func
      unreachable
      i32.const 0
      call 0
    )
    */
    const auto wasm_arg =
        from_hex("0061736d0100000001090260027f7f0060000003030200010a0c0202000b070000410010000b");
    EXPECT_NO_THROW(parse(wasm_arg));

    /* wat2wasm
    (func (param i32 i32))
    (func
      unreachable
      i32.const 0
      i32.const 0
      call 0
    )
    */
    const auto wasm_multi_arg = from_hex(
        "0061736d0100000001090260027f7f0060000003030200010a0e0202000b0900004100410010000b");
    EXPECT_NO_THROW(parse(wasm_multi_arg));

    /* wat2wasm --no-check
    (func (param i32 i32))
    (func
      unreachable
      i64.const 0
      i32.const 0
      call 0
    )
    */
    const auto wasm_invalid = from_hex(
        "0061736d0100000001090260027f7f0060000003030200010a0e0202000b0900004200410010000b");
    EXPECT_THROW_MESSAGE(parse(wasm_invalid), validation_error, "type mismatch");
}

TEST(validation_stack_type, unreachable_drop)
{
    /* wat2wasm
    (func (result i32)
      unreachable
      drop
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a06010400001a0b");
    EXPECT_NO_THROW(parse(wasm));
}

TEST(validation_stack_type, param_type_mismatch)
{
    /* wat2wasm --no-check
    (func (param i32) (result i32)
      local.get 0
      i64.const 0
      i64.add
    )
    */
    const auto wasm_get =
        from_hex("0061736d0100000001060160017f017f030201000a09010700200042007c0b");
    EXPECT_THROW_MESSAGE(parse(wasm_get), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func (param i32)
      i64.const 0
      local.set 0
    )
    */
    const auto wasm_set = from_hex("0061736d0100000001050160017f00030201000a08010600420021000b");
    EXPECT_THROW_MESSAGE(parse(wasm_set), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func (param i32)
      i64.const 0
      local.tee 0
      drop
    )
    */
    const auto wasm_tee = from_hex("0061736d0100000001050160017f00030201000a09010700420022001a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_tee), validation_error, "type mismatch");
}

TEST(validation_stack_type, local_type_mismatch)
{
    /* wat2wasm --no-check
    (func (result i32)
      (local i32)
      local.get 0
      i64.const 0
      i64.add
    )
    */
    const auto wasm_get =
        from_hex("0061736d010000000105016000017f030201000a0b010901017f200042007c0b");
    EXPECT_THROW_MESSAGE(parse(wasm_get), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func
      (local i32)
      i64.const 0
      local.set 0
    )
    */
    const auto wasm_set = from_hex("0061736d01000000010401600000030201000a0a010801017f420021000b");
    EXPECT_THROW_MESSAGE(parse(wasm_set), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func
      (local i32)
      i64.const 0
      local.tee 0
      drop
    )
    */
    const auto wasm_tee =
        from_hex("0061736d01000000010401600000030201000a0b010901017f420022001a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_tee), validation_error, "type mismatch");
}

TEST(validation_stack_type, multi_local_type_mismatch)
{
    /* wat2wasm --no-check
    (func (result i32)
      (local i32 i64)
      local.get 1
      i32.const 0
      i32.add
    )
    */
    const auto wasm_get =
        from_hex("0061736d010000000105016000017f030201000a0d010b02017f017e200141006a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_get), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func
      (local i64 i32)
      i64.const 0
      local.set 1
    )
    */
    const auto wasm_set =
        from_hex("0061736d01000000010401600000030201000a0c010a02017e017f420021010b");
    EXPECT_THROW_MESSAGE(parse(wasm_set), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func
      (local i64 i32)
      i64.const 0
      local.tee 1
      drop
    )
    */
    const auto wasm_tee =
        from_hex("0061736d01000000010401600000030201000a0d010b02017e017f420022011a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_tee), validation_error, "type mismatch");
}

TEST(validation_stack_type, unreachable_local)
{
    /* wat2wasm
    (func (param i32)
      unreachable
      local.set 0
    )
    */
    const auto wasm_set = from_hex("0061736d0100000001050160017f00030201000a070105000021000b");
    EXPECT_NO_THROW(parse(wasm_set));

    /* wat2wasm
    (func (param i32)
      unreachable
      i32.const 0
      local.set 0
    )
    */
    const auto wasm_set2 = from_hex("0061736d0100000001050160017f00030201000a0901070000410021000b");
    EXPECT_NO_THROW(parse(wasm_set2));

    /* wat2wasm
    (func (param i32) (result i32)
      unreachable
      local.tee 0
    )
    */
    const auto wasm_tee = from_hex("0061736d0100000001060160017f017f030201000a070105000022000b");
    EXPECT_NO_THROW(parse(wasm_tee));

    /* wat2wasm
    (func (param i32) (result i32)
      unreachable
      i32.const 0
      local.tee 0
    )
    */
    const auto wasm_tee2 =
        from_hex("0061736d0100000001060160017f017f030201000a0901070000410022000b");
    EXPECT_NO_THROW(parse(wasm_tee2));

    /* wat2wasm --no-check
    (func (param i32)
      unreachable
      i64.const 0
      local.set 0
    )
    */
    const auto wasm_set_invalid =
        from_hex("0061736d0100000001050160017f00030201000a0901070000420021000b");
    EXPECT_THROW_MESSAGE(parse(wasm_set_invalid), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func (param i32) (result i32)
      unreachable
      i64.const 0
      local.tee 0
    )
    */
    const auto wasm_tee_invalid =
        from_hex("0061736d0100000001060160017f017f030201000a0901070000420022000b");
    EXPECT_THROW_MESSAGE(parse(wasm_tee_invalid), validation_error, "type mismatch");
}

TEST(validation_stack_type, global_type_mismatch)
{
    /* wat2wasm --no-check
    (global i32 (i32.const 0))
    (func (result i32)
      global.get 0
      i64.const 0
      i64.add
    )
    */
    const auto wasm_get =
        from_hex("0061736d010000000105016000017f030201000606017f0041000b0a09010700230042007c0b");
    EXPECT_THROW_MESSAGE(parse(wasm_get), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (global (mut i32) (i32.const 0))
    (func
      i64.const 0
      global.set 0
    )
    */
    const auto wasm_set =
        from_hex("0061736d01000000010401600000030201000606017f0141000b0a08010600420024000b");
    EXPECT_THROW_MESSAGE(parse(wasm_set), validation_error, "type mismatch");
}

TEST(validation_stack_type, unreachable_global)
{
    /* wat2wasm
    (global (mut i32) (i32.const 0))
    (func
      unreachable
      global.set 0
    )
    */
    const auto wasm_set =
        from_hex("0061736d01000000010401600000030201000606017f0141000b0a070105000024000b");
    EXPECT_NO_THROW(parse(wasm_set));

    /* wat2wasm
    (global (mut i32) (i32.const 0))
    (func
      unreachable
      i32.const 0
      global.set 0
    )
    */
    const auto wasm_set2 =
        from_hex("0061736d01000000010401600000030201000606017f0141000b0a0901070000410024000b");
    EXPECT_NO_THROW(parse(wasm_set2));

    /* wat2wasm --no-check
    (global (mut i32) (i32.const 0))
    (func
      unreachable
      i64.const 0
      global.set 0
    )
    */
    const auto wasm_set_invalid =
        from_hex("0061736d01000000010401600000030201000606017f0141000b0a0901070000420024000b");
    EXPECT_THROW_MESSAGE(parse(wasm_set_invalid), validation_error, "type mismatch");
}

TEST(validation_stack_type, block_type_mismatch)
{
    /* wat2wasm --no-check
    (func (result i32)
      i64.const 0
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0601040042000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "type mismatch");
}

TEST(validation_stack_type, unreachable_end)
{
    /* wat2wasm
    (func (result i32)
      unreachable
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a05010300000b");
    EXPECT_NO_THROW(parse(wasm));

    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 0
    )
    */
    const auto wasm_match = from_hex("0061736d010000000105016000017f030201000a070105000041000b");
    EXPECT_NO_THROW(parse(wasm_match));

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      i64.const 0
    )
    */
    const auto wasm_mismatch = from_hex("0061736d010000000105016000017f030201000a070105000042000b");
    EXPECT_THROW_MESSAGE(parse(wasm_mismatch), validation_error, "type mismatch");
}

TEST(validation_stack_type, if_type_mismatch)
{
    /* wat2wasm --no-check
    (func (result i32)
      (i32.const 0)
      (if (result i32)
        (then (i64.const 0))
      )
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0b0109004100047f42000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func (result i32)
      (i32.const 0)
      (if (result i32)
        (then (i32.const 0))
        (else (i64.const 0))
      )
    )
    */
    const auto wasm_else =
        from_hex("0061736d010000000105016000017f030201000a0e010c004100047f41000542000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm_else), validation_error, "type mismatch");
}

TEST(validation_stack_type, if_unreachable)
{
    /* wat2wasm
    (func (result i32)
      (i32.const 0)
      (if (result i32)
        (then
          unreachable
          i32.const 0
        )
        (else (i32.const 0))
      )
    )
    */
    const auto wasm_then_unreachable =
        from_hex("0061736d010000000105016000017f030201000a0f010d004100047f0041000541000b0b");
    EXPECT_NO_THROW(parse(wasm_then_unreachable));

    /* wat2wasm
    (func (result i32)
      (i32.const 0)
      (if (result i32)
        (then (i32.const 0))
        (else
          unreachable
          i32.const 0
        )
      )
    )
    */
    const auto wasm_else_unreachable =
        from_hex("0061736d010000000105016000017f030201000a0f010d004100047f4100050041000b0b");
    EXPECT_NO_THROW(parse(wasm_else_unreachable));

    /* wat2wasm --no-check
    (func (result i32)
      (i32.const 0)
      (if (result i32)
        (then
          unreachable
          i64.const 0)
        (else (i32.const 0))
      )
    )
    */
    const auto wasm_then_mismatch =
        from_hex("0061736d010000000105016000017f030201000a0f010d004100047f0042000541000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm_then_mismatch), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func (result i32)
      (i32.const 0)
      (if (result i32)
        (then (i32.const 0))
        (else
          unreachable
          i64.const 0)
      )
    )
    */
    const auto wasm_else_mismatch =
        from_hex("0061736d010000000105016000017f030201000a0f010d004100047f4100050042000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm_else_mismatch), validation_error, "type mismatch");
}

TEST(validation_stack_type, br_type_mismatch)
{
    /* wat2wasm --no-check
    (func (result i32)
      (block (result i32)
        i64.const 0
        br 0
      )
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0b010900027f42000c000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func (result i32)
      (block (result i32)
        i64.const 0
        i32.const 0
        br_if 0
      )
    )
    */
    const auto wasm_br_if =
        from_hex("0061736d010000000105016000017f030201000a0d010b00027f420041000d000b0b");
    EXPECT_THROW_MESSAGE(parse(wasm_br_if), validation_error, "type mismatch");
}

TEST(validation_stack_type, br_if_branch_not_taken)
{
    /* wat2wasm
    (func (result i32)
      (block (result i32)
        i32.const 0
        i32.const 0
        br_if 0
      )
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f030201000a0d010b00027f410041000d000b0b");
    EXPECT_NO_THROW(parse(wasm));

    /* wat2wasm --no-check
    (func (result i32)
      (block
        i32.const 0
        i32.const 0
        br_if 1
      )
      i32.const 0
    )
    */
    const auto wasm_mismatch =
        from_hex("0061736d010000000105016000017f030201000a0f010d000240410041000d010b41000b");
    EXPECT_THROW_MESSAGE(parse(wasm_mismatch), validation_error, "too many results");
}

TEST(validation_stack_type, unreachable_br)
{
    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 0
      br 0
    )
    */
    const auto wasm_br = from_hex("0061736d010000000105016000017f030201000a090107000041000c000b");
    EXPECT_NO_THROW(parse(wasm_br));

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      i64.const 0
      br 0
    )
    */
    const auto wasm_br_mismatch =
        from_hex("0061736d010000000105016000017f030201000a090107000042000c000b");
    EXPECT_THROW_MESSAGE(parse(wasm_br_mismatch), validation_error, "type mismatch");

    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 0
      i32.const 0
      br_if 0
    )
    */
    const auto wasm_br_if =
        from_hex("0061736d010000000105016000017f030201000a0b01090000410041000d000b");
    EXPECT_NO_THROW(parse(wasm_br_if));

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      i64.const 0
      i32.const 0
      br_if 0
    )
    */
    const auto wasm_br_if_mismatch =
        from_hex("0061736d010000000105016000017f030201000a0b01090000420041000d000b");
    EXPECT_THROW_MESSAGE(parse(wasm_br_if_mismatch), validation_error, "type mismatch");
}

TEST(validation_stack_type, br_table_type_mismatch)
{
    /* wat2wasm --no-check
    (func (result i32)
      (block (result i32)
        i64.const 0
        i32.const 0
        br_table 0 1
      )
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f030201000a0f010d00027f420041000e0100010b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "type mismatch");
}

TEST(validation_stack_type, unreachable_br_table)
{
    /* wat2wasm
    (func (result i32)
      (block (result i32)
        unreachable
        i32.const 0
        i32.const 0
        br_table 0 1
      )
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f030201000a10010e00027f00410041000e0100010b0b");
    EXPECT_NO_THROW(parse(wasm));

    /* wat2wasm --no-check
    (func (result i32)
      (block (result i32)
        unreachable
        i64.const 0
        i32.const 0
        br_table 0 1
      )
    )
    */
    const auto wasm_br_mismatch =
        from_hex("0061736d010000000105016000017f030201000a10010e00027f00420041000e0100010b0b");
    EXPECT_THROW_MESSAGE(parse(wasm_br_mismatch), validation_error, "type mismatch");
}

TEST(validation_stack_type, return_type_mismatch)
{
    /* wat2wasm --no-check
    (func (result i32)
      i64.const 0
      return
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0701050042000f0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "type mismatch");
}

TEST(validation_stack_type, unreachable_return)
{
    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 0
      return
    )
    */
    const auto wasm_br = from_hex("0061736d010000000105016000017f030201000a080106000041000f0b");
    EXPECT_NO_THROW(parse(wasm_br));

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      i64.const 0
      return
    )
    */
    const auto wasm_br_if_mismatch =
        from_hex("0061736d010000000105016000017f030201000a080106000042000f0b");
    EXPECT_THROW_MESSAGE(parse(wasm_br_if_mismatch), validation_error, "type mismatch");
}

TEST(validation_stack_type, select_type_mismatch)
{
    /* wat2wasm --no-check
    (func
      (select (i32.const 1) (i64.const 1) (i32.const 1))
      drop
    )
    */
    const auto wasm1 = from_hex("0061736d01000000010401600000030201000a0c010a004101420141011b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm1), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func
      (select (i64.const 1) (i32.const 1) (i32.const 1))
      drop
    )
    */
    const auto wasm2 = from_hex("0061736d01000000010401600000030201000a0c010a004201410141011b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm2), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func
      (select (i32.const 1) (i32.const 1) (i64.const 1))
      drop
    )
    */
    const auto wasm3 = from_hex("0061736d01000000010401600000030201000a0c010a004101410142011b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm3), validation_error, "type mismatch");
}

TEST(validation_stack_type, unreachable_select)
{
    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 1
      i32.const 2
      i32.const 3
      select
    )
    */
    const auto wasm1 =
        from_hex("0061736d010000000105016000017f030201000a0c010a00004101410241031b0b");
    EXPECT_NO_THROW(parse(wasm1));

    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 2
      i32.const 3
      select
    )
    */
    const auto wasm2 = from_hex("0061736d010000000105016000017f030201000a0a01080000410241031b0b");
    EXPECT_NO_THROW(parse(wasm2));

    /* wat2wasm
    (func (result i64)
      unreachable
      i64.const 2
      i32.const 3
      select
    )
    */
    const auto wasm3 = from_hex("0061736d010000000105016000017e030201000a0a01080000420241031b0b");
    EXPECT_NO_THROW(parse(wasm3));

    /* wat2wasm
    (func (result i32)
      unreachable
      i32.const 3
      select
    )
    */
    const auto wasm4 = from_hex("0061736d010000000105016000017f030201000a080106000041031b0b");
    EXPECT_NO_THROW(parse(wasm4));

    /* wat2wasm
    (func (result i32)
      unreachable
      select
    )
    */
    const auto wasm5 = from_hex("0061736d010000000105016000017f030201000a06010400001b0b");
    EXPECT_NO_THROW(parse(wasm5));

    /* wat2wasm --no-check
    (func
      unreachable
      i64.const 1
      i32.const 2
      i32.const 3
      select
      drop
    )
    */
    const auto wasm_mismatch1 =
        from_hex("0061736d01000000010401600000030201000a0d010b00004201410241031b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_mismatch1), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func
      unreachable
      i32.const 1
      i64.const 2
      i32.const 3
      select
      drop
    )
    */
    const auto wasm_mismatch2 =
        from_hex("0061736d01000000010401600000030201000a0d010b00004101420241031b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_mismatch2), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func
      unreachable
      i32.const 1
      i32.const 2
      i64.const 3
      select
      drop
    )
    */
    const auto wasm_mismatch3 =
        from_hex("0061736d01000000010401600000030201000a0d010b00004101410242031b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm_mismatch3), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func (result i32)
      unreachable
      i64.const 0
      i32.const 0
      select
    )
    */
    const auto wasm_mismatch4 =
        from_hex("0061736d010000000105016000017f030201000a0a01080000420041001b0b");
    EXPECT_THROW_MESSAGE(parse(wasm_mismatch4), validation_error, "type mismatch");

    /* wat2wasm --no-check
    (func
      unreachable
      select
    )
    */
    const auto wasm_mismatch5 = from_hex("0061736d01000000010401600000030201000a06010400001b0b");
    EXPECT_THROW_MESSAGE(parse(wasm_mismatch5), validation_error, "too many results");
}