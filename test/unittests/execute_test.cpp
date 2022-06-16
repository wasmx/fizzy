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

TEST(execute, end)
{
    /* wat2wasm
    (func)
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a040102000b");
    EXPECT_THAT(execute(parse(wasm), 0, {}), Result());
}

TEST(execute, drop)
{
    /* wat2wasm
    (func
      (local i32)
      local.get 0
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a09010701017f20001a0b");
    EXPECT_THAT(execute(parse(wasm), 0, {}), Result());
}

TEST(execute, select)
{
    /* wat2wasm
    (func (param i64 i64 i32) (result i64)
      local.get 0
      local.get 1
      local.get 2
      select
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001080160037e7e7f017e030201000a0b0109002000200120021b0b");

    const auto module = parse(wasm);
    EXPECT_THAT(execute(module, 0, {3_u64, 6_u64, 0_u32}), Result(6));
    EXPECT_THAT(execute(module, 0, {3_u64, 6_u64, 1_u32}), Result(3));
    EXPECT_THAT(execute(module, 0, {3_u64, 6_u64, 42_u32}), Result(3));
}

TEST(execute, local_get)
{
    /* wat2wasm
    (func (param i64) (result i64)
      local.get 0
    )
    */
    const auto wasm = from_hex("0061736d0100000001060160017e017e030201000a0601040020000b");
    EXPECT_THAT(execute(parse(wasm), 0, {42_u64}), Result(42));
}

TEST(execute, local_init)
{
    /* wat2wasm
    (func (result i32) (local i32) (local.get 0))
    (func (result i64) (local i64) (local.get 0))
    (func (result f32) (local f32) (local.get 0))
    (func (result f64) (local f64) (local.get 0))
    */
    const auto wasm = from_hex(
        "0061736d010000000111046000017f6000017e6000017d6000017c030504000102030a1d040601017f20000b06"
        "01017e20000b0601017d20000b0601017c20000b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0));
    EXPECT_THAT(execute(parse(wasm), 1, {}), Result(0));
    EXPECT_THAT(execute(parse(wasm), 2, {}), Result(0.0f));
    EXPECT_THAT(execute(parse(wasm), 3, {}), Result(0.0));
}

TEST(execute, local_init_combined)
{
    /* wat2wasm
    (func (result i32) (local i32 i64 f32 f64) (local.get 0))
    (func (result i64) (local i32 i64 f32 f64) (local.get 1))
    (func (result f32) (local i32 i64 f32 f64) (local.get 2))
    (func (result f64) (local i32 i64 f32 f64) (local.get 3))
    */
    const auto wasm = from_hex(
        "0061736d010000000111046000017f6000017e6000017d6000017c030504000102030a35040c04017f017e017d"
        "017c20000b0c04017f017e017d017c20010b0c04017f017e017d017c20020b0c04017f017e017d017c20030b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(0));
    EXPECT_THAT(execute(parse(wasm), 1, {}), Result(0));
    EXPECT_THAT(execute(parse(wasm), 2, {}), Result(0.0f));
    EXPECT_THAT(execute(parse(wasm), 3, {}), Result(0.0));
}

TEST(execute, local_set)
{
    /* wat2wasm
    (func (param i64) (result i64)
      (local i64)
      local.get 0
      local.set 1
      local.get 1
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017e017e030201000a0c010a01017e2000210120010b");
    EXPECT_THAT(execute(parse(wasm), 0, {42_u64}), Result(42));
}

TEST(execute, local_tee)
{
    /* wat2wasm
    (func (param i64) (result i64)
      (local i64)
      local.get 0
      local.tee 1
    )
    */
    const auto wasm = from_hex("0061736d0100000001060160017e017e030201000a0a010801017e200022010b");
    EXPECT_THAT(execute(parse(wasm), 0, {42_u64}), Result(42));
}

TEST(execute, global_get)
{
    /* wat2wasm
    (global i32 (i32.const 42))
    (func (result i32)
      global.get 0
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f030201000606017f00412a0b0a0601040023000b");
    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(42));
}

TEST(execute, global_get_two_globals)
{
    /* wat2wasm
    (global i64 (i64.const 42))
    (global i64 (i64.const 43))
    (func (result i64)
      global.get 0
    )
    (func (result i64)
      global.get 1
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017e0303020000060b027e00422a0b7e00422b0b0a0b02040023000b04002301"
        "0b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Result(42));
    EXPECT_THAT(execute(*instance, 1, {}), Result(43));
}

TEST(execute, global_get_imported)
{
    /* wat2wasm
    (import "mod" "glob" (global i64))
    (func (result i64)
      global.get 0
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017e020d01036d6f6404676c6f62037e00030201000a0601040023000b");

    Value global_value = 42_u64;
    auto instance = instantiate(
        parse(wasm), {}, {}, {}, {ExternalGlobal{&global_value, {ValType::i64, false}}});

    EXPECT_THAT(execute(*instance, 0, {}), Result(42));

    global_value = 0_u64;
    EXPECT_THAT(execute(*instance, 0, {}), Result(0));

    global_value = 43_u64;
    EXPECT_THAT(execute(*instance, 0, {}), Result(43));
}

TEST(execute, global_get_imported_and_internal)
{
    /* wat2wasm
    (module
      (global (import "mod" "g1") i32)
      (global (import "mod" "g2") i32)
      (global i32 (i32.const 42))
      (global i32 (i32.const 43))
      (func (result i32) (global.get 0))
      (func (result i32) (global.get 1))
      (func (result i32) (global.get 2))
      (func (result i32) (global.get 3))
    )
     */
    const auto wasm = from_hex(
        "0061736d010000000105016000017f021502036d6f64026731037f00036d6f64026732037f0003050400000000"
        "060b027f00412a0b7f00412b0b0a1504040023000b040023010b040023020b040023030b");

    Value g1 = 40;
    Value g2 = 41;
    auto instance = instantiate(parse(wasm), {}, {}, {},
        {ExternalGlobal{&g1, {ValType::i32, false}}, ExternalGlobal{&g2, {ValType::i32, false}}});

    EXPECT_THAT(execute(*instance, 0, {}), Result(40));
    EXPECT_THAT(execute(*instance, 1, {}), Result(41));
    EXPECT_THAT(execute(*instance, 2, {}), Result(42));
    EXPECT_THAT(execute(*instance, 3, {}), Result(43));
}

TEST(execute, global_get_float)
{
    /* wat2wasm
    (global (import "m" "g1") (mut f32))
    (global (import "m" "g2") f64)
    (global f32 (f32.const 1.2))
    (global (mut f64) (f64.const 3.4))
    (global f64 (global.get 1))

    (func (result f32) global.get 0)
    (func (result f64) global.get 1)
    (func (result f32) global.get 2)
    (func (result f64) global.get 3)
    (func (result f64) global.get 4)
    */
    const auto wasm = from_hex(
        "0061736d010000000109026000017d6000017c021102016d026731037d01016d026732037c0003060500010001"
        "01061a037d00439a99993f0b7c01443333333333330b400b7c0023010b0a1a05040023000b040023010b040023"
        "020b040023030b040023040b");

    Value g1 = 5.6f;
    Value g2 = 7.8;
    auto instance = instantiate(parse(wasm), {}, {}, {},
        {ExternalGlobal{&g1, {ValType::f32, true}}, ExternalGlobal{&g2, {ValType::f64, false}}});

    EXPECT_THAT(execute(*instance, 0, {}), Result(5.6f));
    EXPECT_THAT(execute(*instance, 1, {}), Result(7.8));
    EXPECT_THAT(execute(*instance, 2, {}), Result(1.2f));
    EXPECT_THAT(execute(*instance, 3, {}), Result(3.4));
    EXPECT_THAT(execute(*instance, 4, {}), Result(7.8));
}

TEST(execute, global_set)
{
    /* wat2wasm
    (global (mut i32) (i32.const 41))
    (func
      i32.const 42
      global.set 0
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000606017f0141290b0a08010600412a24000b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Result());
    EXPECT_EQ(instance->globals[0].i32, 42);
}

TEST(execute, global_set_two_globals)
{
    /* wat2wasm
    (global (mut i32) (i32.const 42))
    (global (mut i32) (i32.const 43))
    (func
      i32.const 44
      global.set 0
      i32.const 45
      global.set 1
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001040160000003020100060b027f01412a0b7f01412b0b0a0c010a00412c2400412d24010"
        "b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Result());
    EXPECT_EQ(instance->globals[0].i32, 44);
    EXPECT_EQ(instance->globals[1].i32, 45);
}

TEST(execute, global_set_imported)
{
    /* wat2wasm
    (import "mod" "glob" (global (mut i32)))
    (func
      i32.const 42
      global.set 0
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000020d01036d6f6404676c6f62037f01030201000a08010600412a24000b");

    Value global_value = 41;
    auto instance =
        instantiate(parse(wasm), {}, {}, {}, {ExternalGlobal{&global_value, {ValType::i32, true}}});
    EXPECT_THAT(execute(*instance, 0, {}), Result());
    EXPECT_EQ(global_value.i32, 42);
}

TEST(execute, global_set_float)
{
    /* wat2wasm
    (global (import "m" "g1") (mut f32))
    (global (import "m" "g2") (mut f64))
    (global (mut f32) (f32.const 1.2))
    (global (mut f64) (f64.const 3.4))

    (func (global.set 0 (f32.const 11.22)))
    (func (global.set 1 (f64.const 33.44)))
    (func (global.set 2 (f32.const 55.66)))
    (func (global.set 3 (f64.const 77.88)))
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000021102016d026731037d01016d026732037c01030504000000000615027d01"
        "439a99993f0b7c01443333333333330b400b0a31040900431f85334124000b0d0044b81e85eb51b8404024010b"
        "090043d7a35e4224020b0d0044b81e85eb5178534024030b");

    Value g1 = 5.6f;
    Value g2 = 7.8;
    auto instance = instantiate(parse(wasm), {}, {}, {},
        {ExternalGlobal{&g1, {ValType::f32, true}}, ExternalGlobal{&g2, {ValType::f64, true}}});

    EXPECT_THAT(execute(*instance, 0, {}), Result());
    EXPECT_EQ(g1.f32, 11.22f);
    EXPECT_THAT(execute(*instance, 1, {}), Result());
    EXPECT_EQ(g2.f64, 33.44);
    EXPECT_THAT(execute(*instance, 2, {}), Result());
    EXPECT_EQ(instance->globals[0].f32, 55.66f);
    EXPECT_THAT(execute(*instance, 3, {}), Result());
    EXPECT_EQ(instance->globals[1].f64, 77.88);
}

TEST(execute, i32_load_imported_memory)
{
    /* wat2wasm
    (import "mod" "m" (memory 1 1))
    (func (param i32) (result i32)
      local.get 0
      i32.load
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f020b01036d6f64016d02010101030201000a0901070020002802000b");

    bytes memory(PageSize, 0);
    auto instance = instantiate(parse(wasm), {}, {}, {{&memory, {1, 1}}});
    memory[1] = 42;
    EXPECT_THAT(execute(*instance, 0, {1}), Result(42));

    EXPECT_THAT(execute(*instance, 0, {65537}), Traps());
}

TEST(execute, i32_load_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result i32)
      local.get 0
      i32.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000504010101010a0d010b0020002802ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0_u32}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute, i64_load_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result i64)
      local.get 0
      i64.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017e030201000504010101010a0d010b0020002903ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0_u32}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute, i32_load_all_variants)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result i32)
      local.get 0
      i32.load  ;; to be replaced by variants of i32.load
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017f030201000504010101010a0901070020002802000b");
    const auto module = parse(wasm);

    auto* const load_instr = const_cast<uint8_t*>(&module->codesec[0].instructions[5]);
    ASSERT_EQ(*load_instr, Instr::i32_load);
    ASSERT_EQ(bytes_view(load_instr + 1, 4), "00000000"_bytes);  // load offset.

    const auto memory_fill = "deb0b1b2b3ed"_bytes;

    constexpr std::tuple<Instr, uint32_t> test_cases[]{
        {Instr::i32_load8_u, 0x000000b0},
        {Instr::i32_load8_s, 0xffffffb0},
        {Instr::i32_load16_u, 0x0000b1b0},
        {Instr::i32_load16_s, 0xffffb1b0},
        {Instr::i32_load, 0xb3b2b1b0},
    };

    for (const auto& [instr, expected] : test_cases)
    {
        *load_instr = static_cast<uint8_t>(instr);
        auto instance = instantiate(*module);
        std::copy(std::begin(memory_fill), std::end(memory_fill), std::begin(*instance->memory));
        EXPECT_THAT(execute(*instance, 0, {1}), Result(expected));

        EXPECT_THAT(execute(*instance, 0, {65537}), Traps());
    }
}

TEST(execute, i64_load_all_variants)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result i64)
      local.get 0
      i64.load  ;; to be replaced by variants of i64.load
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017e030201000504010101010a0901070020002903000b");
    const auto module = parse(wasm);

    auto* const load_instr = const_cast<uint8_t*>(&module->codesec[0].instructions[5]);
    ASSERT_EQ(*load_instr, Instr::i64_load);
    ASSERT_EQ(bytes_view(load_instr + 1, 4), "00000000"_bytes);  // load offset.

    const auto memory_fill = "deb0b1b2b3b4b5b6b7ed"_bytes;

    constexpr std::tuple<Instr, uint64_t> test_cases[]{
        {Instr::i64_load8_u, 0x00000000000000b0},
        {Instr::i64_load8_s, 0xffffffffffffffb0},
        {Instr::i64_load16_u, 0x000000000000b1b0},
        {Instr::i64_load16_s, 0xffffffffffffb1b0},
        {Instr::i64_load32_u, 0x00000000b3b2b1b0},
        {Instr::i64_load32_s, 0xffffffffb3b2b1b0},
        {Instr::i64_load, 0xb7b6b5b4b3b2b1b0},
    };

    for (const auto& [instr, expected] : test_cases)
    {
        *load_instr = static_cast<uint8_t>(instr);
        auto instance = instantiate(*module);
        std::copy(std::begin(memory_fill), std::end(memory_fill), std::begin(*instance->memory));
        EXPECT_THAT(execute(*instance, 0, {1}), Result(expected));

        EXPECT_THAT(execute(*instance, 0, {65537}), Traps());
    }
}

TEST(execute, i32_store_imported_memory)
{
    /* wat2wasm
    (import "mod" "m" (memory 1 1))
    (func (param i32 i32)
      local.get 1
      local.get 0
      i32.store
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027f7f00020b01036d6f64016d02010101030201000a0b01090020012000360200"
        "0b");

    bytes memory(PageSize, 0);
    auto instance = instantiate(parse(wasm), {}, {}, {{&memory, {1, 1}}});
    EXPECT_THAT(execute(*instance, 0, {42, 0}), Result());
    EXPECT_EQ(memory.substr(0, 4), from_hex("2a000000"));

    EXPECT_THAT(execute(*instance, 0, {42, 65537}), Traps());
}

TEST(execute, i32_store_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32)
      local.get 0
      i32.const 0xaa55aa55
      i32.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a13011100200041d5d4d6d27a3602ffffffff07"
        "0b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0_u32}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute, i64_store_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32)
      local.get 0
      i64.const 0xaa55aa55aa55aa55
      i64.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a18011600200042d5d4d6d2dacaeaaaaa7f3703"
        "ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0_u32}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute, i32_store_all_variants)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32 i32)
      local.get 1
      local.get 0
      i32.store  ;; to be replaced by variants of i32.store
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160027f7f00030201000504010101010a0b010900200120003602000b");
    const auto module = parse(wasm);

    auto* const store_instr = const_cast<uint8_t*>(&module->codesec[0].instructions[10]);
    ASSERT_EQ(*store_instr, Instr::i32_store);
    ASSERT_EQ(bytes_view(store_instr + 1, 4), "00000000"_bytes);  // store offset

    const std::tuple<Instr, bytes> test_cases[]{
        {Instr::i32_store8, "ccb0cccccccc"_bytes},
        {Instr::i32_store16, "ccb0b1cccccc"_bytes},
        {Instr::i32_store, "ccb0b1b2b3cc"_bytes},
    };

    for (const auto& [instr, expected] : test_cases)
    {
        *store_instr = static_cast<uint8_t>(instr);
        auto instance = instantiate(*module);
        std::fill_n(instance->memory->begin(), 6, uint8_t{0xcc});
        EXPECT_THAT(execute(*instance, 0, {0xb3b2b1b0, 1}), Result());
        EXPECT_EQ(instance->memory->substr(0, 6), expected);

        EXPECT_THAT(execute(*instance, 0, {0xb3b2b1b0, 65537}), Traps());
    }
}

TEST(execute, i64_store_all_variants)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i64 i32)
      local.get 1
      local.get 0
      i64.store  ;; to be replaced by variants of i64.store
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160027e7f00030201000504010101010a0b010900200120003703000b");
    const auto module = parse(wasm);

    auto* const store_instr = const_cast<uint8_t*>(&module->codesec[0].instructions[10]);
    ASSERT_EQ(*store_instr, Instr::i64_store);
    ASSERT_EQ(bytes_view(store_instr + 1, 4), "00000000"_bytes);  // store offset

    const std::tuple<Instr, bytes> test_cases[]{
        {Instr::i64_store8, "ccb0cccccccccccccccc"_bytes},
        {Instr::i64_store16, "ccb0b1cccccccccccccc"_bytes},
        {Instr::i64_store32, "ccb0b1b2b3cccccccccc"_bytes},
        {Instr::i64_store, "ccb0b1b2b3b4b5b6b7cc"_bytes},
    };

    for (const auto& [instr, expected] : test_cases)
    {
        *store_instr = static_cast<uint8_t>(instr);
        auto instance = instantiate(*module);
        std::fill_n(instance->memory->begin(), 10, uint8_t{0xcc});
        EXPECT_THAT(execute(*instance, 0, {0xb7b6b5b4b3b2b1b0_u64, 1_u32}), Result());
        EXPECT_EQ(instance->memory->substr(0, 10), expected);

        EXPECT_THAT(execute(*instance, 0, {0xb7b6b5b4b3b2b1b0_u64, 65537_u32}), Traps());
    }
}

TEST(execute, memory_size)
{
    /* wat2wasm
    (memory 3 4)
    (func (result i32)
      memory.size
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f030201000504010103040a060104003f000b");

    EXPECT_THAT(execute(parse(wasm), 0, {}), Result(3));
}

TEST(execute, memory_grow)
{
    /* wat2wasm
    (memory 1 4096)
    (func (param i32) (result i32)
      local.get 0
      memory.grow
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017f03020100050501010180200a08010600200040000b");

    const auto module = parse(wasm);

    EXPECT_THAT(execute(module, 0, {0}), Result(1));

    EXPECT_THAT(execute(module, 0, {1}), Result(1));

    // 256MB memory.
    EXPECT_THAT(execute(module, 0, {4095}), Result(1));

    // >256MB memory.
    EXPECT_THAT(execute(module, 0, {4096}), Result(-1));

    // Way too high (but still within bounds)
    EXPECT_THAT(execute(module, 0, {0xffffffe}), Result(-1));
}

TEST(execute, memory_grow_custom_hard_limit)
{
    constexpr std::pair<uint32_t, uint32_t> test_cases[]{
        {0, 1},
        {1, 1},
        {15, 1},
        {16, -1},
        {65535, -1},
        {0xffffffff, -1},
    };

    /* wat2wasm
    (memory 1)
    (func (param i32) (result i32)
      local.get 0
      memory.grow
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017f0302010005030100010a08010600200040000b");
    const auto module = parse(wasm);

    for (const auto& [input, expected] : test_cases)
    {
        const auto instance = instantiate(*module, {}, {}, {}, {}, 16);
        EXPECT_THAT(execute(*instance, 0, {input}), Result(expected));
    }

    {
        const auto instance_huge_hard_limit = instantiate(*module, {}, {}, {}, {}, 65536);
        // For huge hard limit we test only failure cases, because allocating 4GB of memory would
        // be too slow for unit tests.
        if constexpr (sizeof(size_t) < sizeof(uint64_t))
        {
            // On 32-bit 4 GB can't be allocated
            EXPECT_THAT(execute(*instance_huge_hard_limit, 0, {65535}), Result(-1));
        }
        else
        {
            // EXPECT_THAT(execute(*instance_huge_hard_limit, 0, {65535}), Result(1));
        }
        EXPECT_THAT(execute(*instance_huge_hard_limit, 0, {65536}), Result(-1));
        EXPECT_THAT(execute(*instance_huge_hard_limit, 0, {0xffffffff}), Result(-1));
    }

    /* wat2wasm
    (memory 1 16)
    (func (param i32) (result i32)
      local.get 0
      memory.grow
    )
    */
    const auto wasm_max_limit =
        from_hex("0061736d0100000001060160017f017f030201000504010101100a08010600200040000b");
    const auto module_max_limit = parse(wasm_max_limit);

    for (const auto& [input, expected] : test_cases)
    {
        const auto instance = instantiate(*module_max_limit, {}, {}, {}, {}, 32);
        EXPECT_THAT(execute(*instance, 0, {input}), Result(expected));
    }

    /* wat2wasm
    (memory (import "mod" "mem") 1)
    (func (param i32) (result i32)
      local.get 0
      memory.grow
    )
    */
    const auto wasm_imported = from_hex(
        "0061736d0100000001060160017f017f020c01036d6f64036d656d020001030201000a08010600200040000b");
    const auto module_imported = parse(wasm_imported);

    for (const auto& [input, expected] : test_cases)
    {
        bytes memory(PageSize, 0);
        const auto instance =
            instantiate(*module_imported, {}, {}, {{&memory, {1, std::nullopt}}}, {}, 16);
        EXPECT_THAT(execute(*instance, 0, {input}), Result(expected));

        bytes memory_max_limit(PageSize, 0);
        const auto instance_max_limit =
            instantiate(*module_imported, {}, {}, {{&memory_max_limit, {1, 16}}}, {}, 32);
        EXPECT_THAT(execute(*instance_max_limit, 0, {input}), Result(expected));
    }

    {
        bytes memory(PageSize, 0);
        const auto instance_huge_hard_limit =
            instantiate(*module_imported, {}, {}, {{&memory, {1, std::nullopt}}}, {}, 65536);
        EXPECT_THAT(execute(*instance_huge_hard_limit, 0, {65536}), Result(-1));
        EXPECT_THAT(execute(*instance_huge_hard_limit, 0, {0xffffffff}), Result(-1));
    }

    /* wat2wasm
    (memory (import "mod" "mem") 1 16)
    (func (param i32) (result i32)
      local.get 0
      memory.grow
    )
    */
    const auto wasm_imported_max_limit = from_hex(
        "0061736d0100000001060160017f017f020d01036d6f64036d656d02010110030201000a08010600200040000"
        "b");
    const auto module_imported_max_limit = parse(wasm_imported_max_limit);

    for (const auto& [input, expected] : test_cases)
    {
        bytes memory(PageSize, 0);
        const auto instance =
            instantiate(*module_imported_max_limit, {}, {}, {{&memory, {1, 16}}}, {}, 32);
        EXPECT_THAT(execute(*instance, 0, {input}), Result(expected));
    }

    /* wat2wasm
    (memory (import "mod" "mem") 1 32)
    (func (param i32) (result i32)
      local.get 0
      memory.grow
    )
    */
    const auto wasm_imported_max_limit_narrowing = from_hex(
        "0061736d0100000001060160017f017f020d01036d6f64036d656d02010120030201000a08010600200040000"
        "b");
    const auto module_imported_max_limit_narrowing = parse(wasm_imported_max_limit_narrowing);

    for (const auto& [input, expected] : test_cases)
    {
        bytes memory(PageSize, 0);
        const auto instance =
            instantiate(*module_imported_max_limit_narrowing, {}, {}, {{&memory, {1, 16}}}, {}, 32);
        EXPECT_THAT(execute(*instance, 0, {input}), Result(expected));
    }
}

TEST(execute, start_section)
{
    // In this test the start function (index 1) writes a i32 value to the memory
    // and the same is read back in the "main" function (index 0).

    /* wat2wasm
    (memory 1 1)
    (start 1)
    (func (result i32)
      (i32.load (i32.const 0))
    )
    (func
      (i32.store (i32.const 0) (i32.const 42))
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000108026000017f60000003030200010504010101010801010a1302070041002802000b0900"
        "4100412a3602000b");

    auto instance = instantiate(parse(wasm));
    ASSERT_EQ(instance->memory->substr(0, 4), "2a000000"_bytes);  // Start function sets this.

    EXPECT_THAT(execute(*instance, 0, {}), Result(42));
    EXPECT_EQ(instance->memory->substr(0, 4), "2a000000"_bytes);
}

TEST(execute, imported_function)
{
    /* wat2wasm
    (import "mod" "foo" (func (param i32 i32) (result i32)))
    */
    const auto wasm = from_hex("0061736d0100000001070160027f7f017f020b01036d6f6403666f6f0000");
    const auto module = parse(wasm);
    ASSERT_EQ(module->typesec.size(), 1);

    constexpr auto host_foo = [](std::any&, Instance&, const Value* args,
                                  ExecutionContext&) noexcept -> ExecutionResult {
        return Value{args[0].i32 + args[1].i32};
    };

    auto instance = instantiate(*module, {{{host_foo}, module->typesec[0]}});
    EXPECT_THAT(execute(*instance, 0, {20, 22}), Result(42));
}

TEST(execute, imported_two_functions)
{
    /* wat2wasm
    (type (func (param i32 i32) (result i32)))
    (import "mod" "foo1" (func (type 0)))
    (import "mod" "foo2" (func (type 0)))
    */
    const auto wasm = from_hex(
        "0061736d0100000001070160027f7f017f021702036d6f6404666f6f310000036d6f6404666f6f320000");
    const auto module = parse(wasm);
    ASSERT_EQ(module->typesec.size(), 1);

    constexpr auto host_foo1 = [](std::any&, Instance&, const Value* args,
                                   ExecutionContext&) noexcept -> ExecutionResult {
        return Value{args[0].i32 + args[1].i32};
    };
    constexpr auto host_foo2 = [](std::any&, Instance&, const Value* args,
                                   ExecutionContext&) noexcept -> ExecutionResult {
        return Value{args[0].i32 * args[1].i32};
    };

    auto instance = instantiate(
        *module, {{{host_foo1}, module->typesec[0]}, {{host_foo2}, module->typesec[0]}});
    EXPECT_THAT(execute(*instance, 0, {20, 22}), Result(42));
    EXPECT_THAT(execute(*instance, 1, {20, 22}), Result(440));
}

TEST(execute, imported_functions_and_regular_one)
{
    /* wat2wasm
    (type (func (param i32 i32) (result i32)))
    (import "mod" "foo1" (func (type 0)))
    (import "mod" "foo2" (func (type 0)))
    (func (type 0)
      i32.const 0x2a002a
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001070160027f7f017f021702036d6f6404666f6f310000036d6f6404666f6f320000030201"
        "000a0901070041aa80a8010b");

    constexpr auto host_foo1 = [](std::any&, Instance&, const Value* args,
                                   ExecutionContext&) noexcept -> ExecutionResult {
        return Value{args[0].i32 + args[1].i32};
    };
    constexpr auto host_foo2 = [](std::any&, Instance&, const Value* args,
                                   ExecutionContext&) noexcept -> ExecutionResult {
        return Value{args[0].i32 * args[1].i32};
    };

    const auto module = parse(wasm);
    ASSERT_EQ(module->typesec.size(), 1);
    auto instance = instantiate(
        *module, {{{host_foo1}, module->typesec[0]}, {{host_foo2}, module->typesec[0]}});
    EXPECT_THAT(execute(*instance, 0, {20, 22}), Result(42));
    EXPECT_THAT(execute(*instance, 1, {20, 10}), Result(200));
}

TEST(execute, imported_two_functions_different_type)
{
    /* wat2wasm
    (type (func (param i32 i32) (result i32)))
    (type (func (param i64) (result i64)))
    (import "mod" "foo1" (func (type 0)))
    (import "mod" "foo2" (func (type 1)))
    (func (type 1)
      i64.const 0x2a002a
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260027f7f017f60017e017e021702036d6f6404666f6f310000036d6f6404666f6f32"
        "0001030201010a0901070042aa80a8010b");

    constexpr auto host_foo1 = [](std::any&, Instance&, const Value* args,
                                   ExecutionContext&) noexcept -> ExecutionResult {
        return Value{args[0].i32 + args[1].i32};
    };
    constexpr auto host_foo2 = [](std::any&, Instance&, const Value* args,
                                   ExecutionContext&) noexcept -> ExecutionResult {
        return Value{args[0].i64 * args[0].i64};
    };

    const auto module = parse(wasm);
    ASSERT_EQ(module->typesec.size(), 2);
    auto instance = instantiate(
        *module, {{{host_foo1}, module->typesec[0]}, {{host_foo2}, module->typesec[1]}});

    EXPECT_THAT(execute(*instance, 0, {20_u32, 22_u32}), Result(42));
    EXPECT_THAT(execute(*instance, 1, {0x3000'0000_u64}), Result(0x900'0000'0000'0000));
    EXPECT_THAT(execute(*instance, 2, {20_u64}), Result(0x2a002a));
}

TEST(execute, imported_function_traps)
{
    /* wat2wasm
    (import "mod" "foo" (func (param i32 i32) (result i32)))
    */
    const auto wasm = from_hex("0061736d0100000001070160027f7f017f020b01036d6f6403666f6f0000");

    constexpr auto host_foo = [](std::any&, Instance&, const Value*, ExecutionContext&) noexcept {
        return Trap;
    };

    const auto module = parse(wasm);
    auto instance = instantiate(*module, {{{host_foo}, module->typesec[0]}});
    EXPECT_THAT(execute(*instance, 0, {20, 22}), Traps());
}

TEST(execute, memory_copy_32bytes)
{
    /* wat2wasm
    (memory 1)

    ;; copy32(dst, src) - copies 4 x 8 bytes using offset immediate.
    (func (param i32 i32)
      local.get 0
      local.get 1
      i64.load offset=0
      i64.store offset=0
      local.get 0
      local.get 1
      i64.load offset=8
      i64.store offset=8
      local.get 0
      local.get 1
      i64.load offset=16
      i64.store offset=16
      local.get 0
      local.get 1
      i64.load offset=24
      i64.store offset=24
    )
    */

    const auto bin = from_hex(
        "0061736d0100000001060160027f7f000302010005030100010a2c012a00200020012903003703002000200129"
        "030837030820002001290310370310200020012903183703180b");

    auto instance = instantiate(parse(bin));
    ASSERT_EQ(instance->memory->size(), 65536);
    const auto input = from_hex("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
    ASSERT_EQ(input.size(), 32);
    std::copy(input.begin(), input.end(), instance->memory->begin());
    EXPECT_THAT(execute(*instance, 0, {33, 0}), Result());
    ASSERT_EQ(instance->memory->size(), 65536);
    bytes output;
    std::copy_n(&(*instance->memory)[33], input.size(), std::back_inserter(output));
    EXPECT_EQ(output, input);
}

TEST(execute, reuse_args)
{
    /* wat2wasm
    (func $f (param i64 i64) (result i64)
      local.get 0
      local.get 1
      i64.div_u
      local.set 1
      local.get 0
      local.get 1
      i64.rem_u
    )
    (func (result i64)
      i64.const 23
      i64.const 5
      call $f
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010b0260027e7e017e6000017e03030200010a19020e002000200180210120002001820b08"
        "004217420510000b");

    auto instance = instantiate(parse(wasm));

    const std::vector<Value> args{20_u64, 3_u64};
    const auto expected = args[0].i64 % (args[0].i64 / args[1].i64);
    EXPECT_THAT(
        TypedExecutionResult(execute(*instance, 0, args.data()), ValType::i64), Result(expected));
    EXPECT_THAT(args[0].i64, 20);
    EXPECT_THAT(args[1].i64, 3);

    EXPECT_THAT(execute(parse(wasm), 1, {}), Result(23 % (23 / 5)));
}

TEST(execute, stack_abuse)
{
    /* wat2wasm
    (func (param i32) (result i32)
      local.get 0
      i32.const 1
      i32.const 2
      i32.const 3
      i32.const 4
      i32.const 5
      i32.const 6
      i32.const 7
      i32.const 8
      i32.const 9
      i32.const 10
      i32.const 11
      i32.const 12
      i32.const 13
      i32.const 14
      i32.const 15
      i32.const 16
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
      i32.add
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000a360134002000410141024103410441054106410741084109"
        "410a410b410c410d410e410f41106a6a6a6a6a6a6a6a6a6a6a6a6a6a6a6a0b");

    EXPECT_THAT(execute(parse(wasm), 0, {1000}), Result(1136));
}

TEST(execute, metering)
{
    /* wat2wasm
    (func (result i32)
      i32.const 1
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017f030201000a0601040041010b");
    auto instance = instantiate(parse(wasm));

    ExecutionContext ctx;
    ctx.metering_enabled = true;
    ctx.ticks = 100;
    EXPECT_THAT(execute(*instance, 0, {}, ctx), Result(1_u32));
    EXPECT_EQ(ctx.ticks, 98);

    ctx.ticks = 2;
    EXPECT_THAT(execute(*instance, 0, {}, ctx), Result(1_u32));
    EXPECT_EQ(ctx.ticks, 0);

    ctx.ticks = 1;
    EXPECT_THAT(execute(*instance, 0, {}, ctx), Traps());

    ctx.ticks = 0;
    EXPECT_THAT(execute(*instance, 0, {}, ctx), Traps());
}
