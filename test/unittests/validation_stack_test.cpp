#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;

TEST(validation_stack, func_stack_underflow)
{
    /* wat2wasm --no-check
    (func (param i32 i32) (result i32)
      get_local 0
      get_local 1
      i32.add
      i32.add
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001070160027f7f017f030201000a0a010800200020016a6a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, block_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      i32.const 2
      (block
        drop
      )
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0a010800410202401a0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, block_with_result)
{
    /* wat2wasm
    (func
      (block (result i32)
        i32.const -1
      )
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0a010800027f417f0b1a0b");
    parse(wasm);
    // TODO: Add max stack height check.
}

TEST(validation_stack, block_with_result_stack_underflow)
{
    /* wat2wasm --no-check
    (func (result i32)
      (block (result i32)
        i32.const -1
      )
      i32.add
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0a010800027f417f0b6a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, loop_stack_underflow)
{
    /* wat2wasm --no-check
    (func (param i32)
      get_local 0
      (loop
        i32.eqz
        drop
      )
    )
    */
    const auto wasm = from_hex("0061736d0100000001050160017f00030201000a0b01090020000340451a0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, loop_with_result)
{
    /* wat2wasm
    (func
      (loop (result i32)
        i32.const -1
      )
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0a010800037f417f0b1a0b");
    parse(wasm);
    // TODO: Add max stack height check.
}

TEST(validation_stack, loop_with_result_stack_underflow)
{
    /* wat2wasm --no-check
    (func (result i32)
      (loop (result i32)
        i32.const -1
      )
      i32.add
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0a010800037f417f0b6a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, DISABLED_call_stack_underflow)
{
    /* wat2wasm --no-check
    (func $f (param i32) (result i32)
      get_local 0
    )
    (func (result i32)
      ;; Call argument missing.
      call $f
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010a0260017f017f6000017f03030200010a0b02040020000b040010000b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, unreachable)
{
    /* wat2wasm
    (func (result i32)
      unreachable
      i32.eqz
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0601040000450b");
    parse(wasm);
    // TODO: Add max stack height check.
}

TEST(validation_stack, unreachable_2)
{
    /* wat2wasm
    (func
      unreachable
      i32.add
      i32.add
      i32.add
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a09010700006a6a6a1a0b");
    parse(wasm);
    // TODO: Add max stack height check.
}

TEST(validation_stack, br)
{
    /* wat2wasm
    (func
      (block
        br 0
        i32.eqz  ;; unreachable
        drop
      )
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a0b01090002400c00451a0b0b");
    parse(wasm);
    // TODO: Add max stack height check.
}

TEST(validation_stack, br_table)
{
    /* wat2wasm
    (func (param i32)
      (block
        i32.const 1001
        get_local 0
        br_table 0 1
        i32.mul  ;; unreachable
        i32.mul
        i32.mul
        drop
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000a14011200024041e90720000e0100016c6c6c1a0b0b");
    parse(wasm);
    // TODO: Add max stack height check.
}

TEST(validation_stack, return_)
{
    /* wat2wasm
    (func
      return
      i32.eqz  ;; unreachable
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a070105000f451a0b");
    parse(wasm);
    // TODO: Add max stack height check.
}

TEST(validation_stack, if_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      (local i64)
      i64.const 1
      i32.const 2
      (if
        (then
          set_local 0  ;; stack underflow
        )
      )
      drop
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a10010e01017e42014102044021000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, else_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      (local i64)
      i64.const 1
      i32.const 2
      (if
        (then)
        (else
          set_local 0  ;; stack underflow
        )
      )
      drop
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a11010f01017e4201410204400521000b1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, if_with_result_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      (local i64)
      i64.const 1
      i32.const 2
      (if (result i64)
        (then
          set_local 0  ;; stack underflow
          i64.const -1
        )
        (else
          i64.const -2
        )
      )
      drop
      drop
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a16011401017e42014102047e2100427f05427e0b1a1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, else_with_result_stack_underflow)
{
    /* wat2wasm --no-check
    (func
      (local i64)
      i64.const 1
      i32.const 2
      (if (result i64)
        (then
          i64.const -1
        )
        (else
          set_local 0  ;; stack underflow
          i64.const -2
        )
      )
      drop
      drop
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a16011401017e42014102047e427f052100427e0b1a1a0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}

TEST(validation_stack, if_invalid_end_stack_height)
{
    /* wat2wasm --no-check
    (func
      i64.const 1
      i32.const 2
      (if (result i64)
        (then
          i64.const 1
          i64.const 2  ;; Stack height 2, but should be 1.
        )
        (else
          i64.const 3
          i64.const 4
          drop
        )
      )
      drop
      drop
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a1701150042014102047e4201420205420342041a0b1a1a0b");
    parse(wasm);
    // TODO: Add max stack height check.
}

TEST(validation_stack, if_with_unreachable)
{
    /* wat2wasm --no-check
    (func (param i32) (result i64)
      get_local 0
      (if (result i64)
        (then
          unreachable
          i64.const 1
        )
        (else
          drop ;; Stack underflow.
        )
      )
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017e030201000a0e010c002000047e004201051a0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "stack underflow");
}
