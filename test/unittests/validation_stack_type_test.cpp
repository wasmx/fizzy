// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

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
