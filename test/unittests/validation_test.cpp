#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;

TEST(validation, i32_store_no_memory)
{
    /* wat2wasm --no-check
    (func (param i32)
      get_local 0
      i32.const 0
      i32.store
    )
    */
    const auto wasm = from_hex("0061736d0100000001050160017f00030201000a0b010900200041003602000b");
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "memory instructions require imported or defined memory");
}

TEST(validation, f32_store_no_memory)
{
    /* wat2wasm --no-check
    (func (param f32)
      get_local 0
      f32.const 0
      f32.store
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001050160017d00030201000a0e010c00200043000000003802000b");
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "memory instructions require imported or defined memory");
}

TEST(validation, memory_size_no_memory)
{
    /* wat2wasm --no-check
    (func (result i32)
      memory.size
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a060104003f000b");
    EXPECT_THROW_MESSAGE(
        parse(wasm), validation_error, "memory instructions require imported or defined memory");
}

TEST(validation, br_invalid_label_index)
{
    /* wat2wasm --no-check
    (func
      br 1
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a060104000c010b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid label index");
}

TEST(validation, br_if_invalid_label_index)
{
    /* wat2wasm --no-check
    (func
      (block
        (loop
           i32.const 0
           br_if 3
        )
      )
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000a0e010c000240034041000d030b0b0b");
    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid label index");
}