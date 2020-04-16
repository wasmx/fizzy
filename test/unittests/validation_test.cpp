#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>
#include <test/utils/wasm_binary.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(validation, import_memories_multiple)
{
    const auto section_contents =
        make_vec({bytes{0x02, 'm', '1', 0x03, 'a', 'b', 'c', 0x02, 0x00, 0x7f},
            bytes{0x02, 'm', '2', 0x03, 'd', 'e', 'f', 0x02, 0x00, 0x7f}});
    const auto bin = bytes{wasm_prefix} + make_section(2, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "too many imported memories (at most one is allowed)");
}

TEST(validation, memory_and_imported_memory)
{
    // (import "js" "mem"(memory 1))
    const auto import_section = "020b01026a73036d656d0200010008046e616d65020100"_bytes;
    // (memory 1)
    const auto memory_section = "05030100010008046e616d65020100"_bytes;
    const auto bin = bytes{wasm_prefix} + import_section + memory_section;

    EXPECT_THROW_MESSAGE(parse(bin), validation_error,
        "both module memory and imported memory are defined (at most one of them is allowed)");
}

TEST(validation, memory_multi_min_limit)
{
    const auto section_contents = "02007f007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(5, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "too many memory sections (at most one is allowed)");
}

TEST(validation, import_tables_multiple)
{
    const auto section_contents =
        make_vec({bytes{0x02, 'm', '1', 0x03, 'a', 'b', 'c', 0x01, 0x70, 0x00, 0x01},
            bytes{0x02, 'm', '2', 0x03, 'd', 'e', 'f', 0x01, 0x70, 0x01, 0x01, 0x03}});
    const auto bin = bytes{wasm_prefix} + make_section(2, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "too many imported tables (at most one is allowed)");
}

TEST(validation, table_and_imported_table)
{
    // (import "js" "t" (table 1 anyfunc))
    const auto import_section = "020a01026a730174017000010008046e616d65020100"_bytes;
    // (table 2 anyfunc)
    const auto table_section = "0404017000020008046e616d65020100"_bytes;
    const auto bin = bytes{wasm_prefix} + import_section + table_section;

    EXPECT_THROW_MESSAGE(parse(bin), validation_error,
        "both module table and imported table are defined (at most one of them is allowed)");
}

TEST(validation, table_multi_min_limit)
{
    const auto section_contents = "0270007f70007f"_bytes;
    const auto bin = bytes{wasm_prefix} + make_section(4, section_contents);

    EXPECT_THROW_MESSAGE(
        parse(bin), validation_error, "too many table sections (at most one is allowed)");
}

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

TEST(validation, br_table_invalid_label_index)
{
    /* wat2wasm --no-check
    (func
      (block
        (block
          (block
            (block
              (block
                (br_table 0 1 2 3 4 5 6 0 (i32.const 0))
              )
            )
          )
        )
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a1f011d000240024002400240024041000e070001020304050600"
        "0b0b0b0b0b0b");

    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid label index");
}

TEST(validation, br_table_default_invalid_label_index)
{
    /* wat2wasm --no-check
    (func
      (block
        (br_table 0 1 0 1 0 1 0 1 2 (i32.const 0))
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000a14011200024041000e080001000100010001020b0b");

    EXPECT_THROW_MESSAGE(parse(wasm), validation_error, "invalid label index");
}