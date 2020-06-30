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
