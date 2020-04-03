#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;

TEST(execute, end)
{
    /* wat2wasm
    (func)
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a040102000b");
    const auto [trap, ret] = execute(parse(wasm), 0, {});
    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, drop)
{
    /* wat2wasm
    (func
      (local i32)
      get_local 0
      drop
    )
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000a09010701017f20001a0b");
    const auto [trap, ret] = execute(parse(wasm), 0, {});
    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, select)
{
    /* wat2wasm
    (func (param i64 i64 i32) (result i64)
      get_local 0
      get_local 1
      get_local 2
      select
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001080160037e7e7f017e030201000a0b0109002000200120021b0b");

    const auto module = parse(wasm);
    EXPECT_RESULT(execute(module, 0, {3, 6, 0}), 6);
    EXPECT_RESULT(execute(module, 0, {3, 6, 1}), 3);
    EXPECT_RESULT(execute(module, 0, {3, 6, 42}), 3);
}

TEST(execute, local_get)
{
    /* wat2wasm
    (func (param i64) (result i64)
      get_local 0
    )
    */
    const auto wasm = from_hex("0061736d0100000001060160017e017e030201000a0601040020000b");
    EXPECT_RESULT(execute(parse(wasm), 0, {42}), 42);
}

TEST(execute, local_set)
{
    /* wat2wasm
    (func (param i64) (result i64)
      (local i64)
      get_local 0
      set_local 1
      get_local 1
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017e017e030201000a0c010a01017e2000210120010b");
    EXPECT_RESULT(execute(parse(wasm), 0, {42}), 42);
}

TEST(execute, local_tee)
{
    /* wat2wasm
    (func (param i64) (result i64)
      (local i64)
      get_local 0
      tee_local 1
    )
    */
    const auto wasm = from_hex("0061736d0100000001060160017e017e030201000a0a010801017e200022010b");
    EXPECT_RESULT(execute(parse(wasm), 0, {42}), 42);
}

TEST(execute, global_get)
{
    /* wat2wasm
    (global i32 (i32.const 42))
    (func (result i32)
      get_global 0
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017f030201000606017f00412a0b0a0601040023000b");
    EXPECT_RESULT(execute(parse(wasm), 0, {}), 42);
}

TEST(execute, global_get_two_globals)
{
    /* wat2wasm
    (global i64 (i64.const 42))
    (global i64 (i64.const 43))
    (func (result i64)
      get_global 0
    )
    (func (result i64)
      get_global 1
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017e0303020000060b027e00422a0b7e00422b0b0a0b02040023000b04002301"
        "0b");

    auto instance = instantiate(parse(wasm));
    EXPECT_RESULT(execute(*instance, 0, {}), 42);
    EXPECT_RESULT(execute(*instance, 1, {}), 43);
}

TEST(execute, global_get_imported)
{
    /* wat2wasm
    (import "mod" "glob" (global i64))
    (func (result i64)
      get_global 0
    )
    */
    const auto wasm = from_hex(
        "0061736d010000000105016000017e020d01036d6f6404676c6f62037e00030201000a0601040023000b");
    const auto module = parse(wasm);

    uint64_t global_value = 42;
    auto instance = instantiate(module, {}, {}, {}, {ExternalGlobal{&global_value, false}});

    EXPECT_RESULT(execute(*instance, 0, {}), 42);

    global_value = 0;
    EXPECT_RESULT(execute(*instance, 0, {}), 0);

    global_value = 43;
    EXPECT_RESULT(execute(*instance, 0, {}), 43);
}

TEST(execute, global_get_imported_and_internal)
{
    /* wat2wasm
    (module
      (global (import "mod" "g1") i32)
      (global (import "mod" "g2") i32)
      (global i32 (i32.const 42))
      (global i32 (i32.const 43))
      (func (param i32) (result i32) (get_global 0))
      (func (param i32) (result i32) (get_global 1))
      (func (param i32) (result i32) (get_global 2))
      (func (param i32) (result i32) (get_global 3))
    )
     */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f021502036d6f64026731037f00036d6f64026732037f00030504000000"
        "00060b027f00412a0b7f00412b0b0a1504040023000b040023010b040023020b040023030b");
    const auto module = parse(wasm);

    uint64_t g1 = 40;
    uint64_t g2 = 41;
    auto instance =
        instantiate(module, {}, {}, {}, {ExternalGlobal{&g1, false}, ExternalGlobal{&g2, false}});

    EXPECT_RESULT(execute(*instance, 0, {}), 40);
    EXPECT_RESULT(execute(*instance, 1, {}), 41);
    EXPECT_RESULT(execute(*instance, 2, {}), 42);
    EXPECT_RESULT(execute(*instance, 3, {}), 43);
}

TEST(execute, global_set)
{
    /* wat2wasm
    (global (mut i32) (i32.const 41))
    (func
      i32.const 42
      set_global 0
    )
    */
    const auto wasm =
        from_hex("0061736d01000000010401600000030201000606017f0141290b0a08010600412a24000b");

    auto instance = instantiate(parse(wasm));
    const auto [trap, _] = execute(*instance, 0, {});
    ASSERT_FALSE(trap);
    EXPECT_EQ(instance->globals[0], 42);
}

TEST(execute, global_set_two_globals)
{
    /* wat2wasm
    (global (mut i32) (i32.const 42))
    (global (mut i32) (i32.const 43))
    (func
      i32.const 44
      set_global 0
      i32.const 45
      set_global 1
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001040160000003020100060b027f01412a0b7f01412b0b0a0c010a00412c2400412d24010"
        "b");

    auto instance = instantiate(parse(wasm));
    const auto [trap, _] = execute(*instance, 0, {});
    ASSERT_FALSE(trap);
    EXPECT_EQ(instance->globals[0], 44);
    EXPECT_EQ(instance->globals[1], 45);
}

TEST(execute, global_set_imported)
{
    /* wat2wasm
    (import "mod" "glob" (global (mut i32)))
    (func
      i32.const 42
      set_global 0
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000020d01036d6f6404676c6f62037f01030201000a08010600412a24000b");

    uint64_t global_value = 41;
    auto instance = instantiate(parse(wasm), {}, {}, {}, {ExternalGlobal{&global_value, true}});
    const auto [trap, _] = execute(*instance, 0, {});
    ASSERT_FALSE(trap);
    EXPECT_EQ(global_value, 42);
}

TEST(execute, i32_load)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 42;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 42);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i32_load_imported_memory)
{
    /* wat2wasm
    (import "mod" "m" (memory 1 1))
    (func (param i32) (result i32)
      get_local 0
      i32.load
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f020b01036d6f64016d02010101030201000a0901070020002802000b");

    bytes memory(PageSize, 0);
    auto instance = instantiate(parse(wasm), {}, {}, {{&memory, {1, 1}}});
    memory[1] = 42;
    EXPECT_RESULT(execute(*instance, 0, {1}), 42);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i32_load_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result i32)
      get_local 0
      i32.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017f030201000504010101010a0d010b0020002802ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    ASSERT_TRUE(execute(*instance, 0, {0}).trapped);
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    ASSERT_TRUE(execute(*instance, 0, {0x80000000}).trapped);
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    ASSERT_TRUE(execute(*instance, 0, {0x80000001}).trapped);
}

TEST(execute, i64_load)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 0x2a;
    (*instance->memory)[4] = 0x2a;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 0x2a0000002a);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i64_load_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result i64)
      get_local 0
      i64.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017e030201000504010101010a0d010b0020002903ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    ASSERT_TRUE(execute(*instance, 0, {0}).trapped);
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    ASSERT_TRUE(execute(*instance, 0, {0x80000000}).trapped);
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    ASSERT_TRUE(execute(*instance, 0, {0x80000001}).trapped);
}

TEST(execute, i32_load8_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load8_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 0x80;
    (*instance->memory)[1] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(static_cast<int32_t>(ret[0]), -128);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i32_load8_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load8_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 0x81;
    (*instance->memory)[1] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0x01);
    ASSERT_EQ(static_cast<uint32_t>(ret[0]), 129);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i32_load16_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load16_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 0x00;
    (*instance->memory)[1] = 0x80;
    (*instance->memory)[3] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(static_cast<int32_t>(ret[0]), -32768);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i32_load16_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load16_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 0x01;
    (*instance->memory)[1] = 0x80;
    (*instance->memory)[3] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0x01);
    ASSERT_EQ(static_cast<uint32_t>(ret[0]), 32769);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i64_load8_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load8_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 0x80;
    (*instance->memory)[1] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], uint64_t(-128));

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i64_load8_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load8_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 0x81;
    (*instance->memory)[1] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 0x81);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i64_load16_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load16_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 0x00;
    (*instance->memory)[1] = 0x80;
    (*instance->memory)[2] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], uint64_t(-32768));

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i64_load16_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load16_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance->memory)[0] = 0x01;
    (*instance->memory)[1] = 0x80;
    (*instance->memory)[2] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 0x8001);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i64_load32_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load32_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    auto& memory = *instance->memory;
    memory[0] = 0x00;
    memory[1] = 0x00;
    memory[2] = 0x00;
    memory[3] = 0x80;
    memory[4] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], uint64_t(-2147483648));

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i64_load32_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load32_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    auto& memory = *instance->memory;
    memory[0] = 0x01;
    memory[1] = 0x00;
    memory[2] = 0x00;
    memory[3] = 0x80;
    memory[4] = 0xf1;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 0x80000001);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i32_store_imported_memory)
{
    /* wat2wasm
    (import "mod" "m" (memory 1 1))
    (func (param i32 i32)
      get_local 1
      get_local 0
      i32.store
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027f7f00020b01036d6f64016d02010101030201000a0b01090020012000360200"
        "0b");

    bytes memory(PageSize, 0);
    auto instance = instantiate(parse(wasm), {}, {}, {{&memory, {1, 1}}});
    const auto [trap, ret] = execute(*instance, 0, {42, 0});
    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    EXPECT_EQ(memory.substr(0, 4), from_hex("2a000000"));

    EXPECT_TRUE(execute(*instance, 0, {42, 65537}).trapped);
}

TEST(execute, i32_store_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32)
      get_local 0
      i32.const 0xaa55aa55
      i32.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a13011100200041d5d4d6d27a3602ffffffff07"
        "0b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    ASSERT_TRUE(execute(*instance, 0, {0}).trapped);
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    ASSERT_TRUE(execute(*instance, 0, {0x80000000}).trapped);
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    ASSERT_TRUE(execute(*instance, 0, {0x80000001}).trapped);
}

TEST(execute, i64_store_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32)
      get_local 0
      i64.const 0xaa55aa55aa55aa55
      i64.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a18011600200042d5d4d6d2dacaeaaaaa7f3703"
        "ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    ASSERT_TRUE(execute(*instance, 0, {0}).trapped);
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    ASSERT_TRUE(execute(*instance, 0, {0x80000000}).trapped);
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    ASSERT_TRUE(execute(*instance, 0, {0x80000001}).trapped);
}

TEST(execute, i32_store_all_variants)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32 i32)
      get_local 1
      get_local 0
      i32.store  ;; to be replaced by variants of i32.store
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160027f7f00030201000504010101010a0b010900200120003602000b");
    auto module = parse(wasm);

    auto& store_instr = module.codesec[0].instructions[2];
    ASSERT_EQ(store_instr, Instr::i32_store);
    ASSERT_EQ(module.codesec[0].immediates.substr(8), "00000000"_bytes);  // store offset

    const std::tuple<Instr, bytes> test_cases[]{
        {Instr::i32_store8, "ccb0cccccccc"_bytes},
        {Instr::i32_store16, "ccb0b1cccccc"_bytes},
        {Instr::i32_store, "ccb0b1b2b3cc"_bytes},
    };

    for (const auto& test_case : test_cases)
    {
        store_instr = std::get<0>(test_case);
        auto instance = instantiate(module);
        std::fill_n(instance->memory->begin(), 6, uint8_t{0xcc});
        const auto [trap, ret] = execute(*instance, 0, {0xb3b2b1b0, 1});
        ASSERT_FALSE(trap);
        EXPECT_EQ(ret.size(), 0);
        EXPECT_EQ(instance->memory->substr(0, 6), std::get<1>(test_case));

        EXPECT_TRUE(execute(*instance, 0, {0xb3b2b1b0, 65537}).trapped);
    }
}

TEST(execute, i64_store_all_variants)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i64 i32)
      get_local 1
      get_local 0
      i64.store  ;; to be replaced by variants of i64.store
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160027e7f00030201000504010101010a0b010900200120003703000b");
    auto module = parse(wasm);

    auto& store_instr = module.codesec[0].instructions[2];
    ASSERT_EQ(store_instr, Instr::i64_store);
    ASSERT_EQ(module.codesec[0].immediates.substr(8), "00000000"_bytes);  // store offset

    const std::tuple<Instr, bytes> test_cases[]{
        {Instr::i64_store8, "ccb0cccccccccccccccc"_bytes},
        {Instr::i64_store16, "ccb0b1cccccccccccccc"_bytes},
        {Instr::i64_store32, "ccb0b1b2b3cccccccccc"_bytes},
        {Instr::i64_store, "ccb0b1b2b3b4b5b6b7cc"_bytes},
    };

    for (const auto& test_case : test_cases)
    {
        store_instr = std::get<0>(test_case);
        auto instance = instantiate(module);
        std::fill_n(instance->memory->begin(), 10, uint8_t{0xcc});
        const auto [trap, ret] = execute(*instance, 0, {0xb7b6b5b4b3b2b1b0, 1});
        ASSERT_FALSE(trap);
        EXPECT_EQ(ret.size(), 0);
        EXPECT_EQ(instance->memory->substr(0, 10), std::get<1>(test_case));

        EXPECT_TRUE(execute(*instance, 0, {0xb7b6b5b4b3b2b1b0, 65537}).trapped);
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

    EXPECT_RESULT(execute(parse(wasm), 0, {}), 3);
}

TEST(execute, memory_grow)
{
    /* wat2wasm
    (memory 1 4096)
    (func (param i32) (result i32)
      get_local 0
      memory.grow
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017f03020100050501010180200a08010600200040000b");

    const auto module = parse(wasm);

    EXPECT_RESULT(execute(module, 0, {0}), 1);

    EXPECT_RESULT(execute(module, 0, {1}), 1);

    // 256MB memory.
    EXPECT_RESULT(execute(module, 0, {4095}), 1);

    // >256MB memory.
    EXPECT_RESULT(execute(module, 0, {4096}), uint32_t(-1));

    // Way too high (but still within bounds)
    EXPECT_RESULT(execute(module, 0, {0xffffffe}), uint32_t(-1));
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

    EXPECT_RESULT(execute(*instance, 0, {}), 42);
    EXPECT_EQ(instance->memory->substr(0, 4), "2a000000"_bytes);
}

TEST(execute, imported_function)
{
    /* wat2wasm
    (import "mod" "foo" (func (param i32 i32) (result i32)))
    */
    const auto wasm = from_hex("0061736d0100000001070160027f7f017f020b01036d6f6403666f6f0000");
    const auto module = parse(wasm);
    ASSERT_EQ(module.typesec.size(), 1);

    constexpr auto host_foo = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };

    auto instance = instantiate(module, {{host_foo, module.typesec[0]}});
    EXPECT_RESULT(execute(*instance, 0, {20, 22}), 42);
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
    ASSERT_EQ(module.typesec.size(), 1);

    constexpr auto host_foo1 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };
    constexpr auto host_foo2 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * args[1]}};
    };

    auto instance =
        instantiate(module, {{host_foo1, module.typesec[0]}, {host_foo2, module.typesec[0]}});
    EXPECT_RESULT(execute(*instance, 0, {20, 22}), 42);
    EXPECT_RESULT(execute(*instance, 1, {20, 22}), 440);
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

    constexpr auto host_foo1 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };
    constexpr auto host_foo2 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * args[0]}};
    };

    const auto module = parse(wasm);
    ASSERT_EQ(module.typesec.size(), 1);
    auto instance =
        instantiate(module, {{host_foo1, module.typesec[0]}, {host_foo2, module.typesec[0]}});
    EXPECT_RESULT(execute(*instance, 0, {20, 22}), 42);
    EXPECT_RESULT(execute(*instance, 1, {20}), 400);

    // check correct number of arguments is passed to host
    constexpr auto count_args = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args.size()}};
    };

    auto instance_counter =
        instantiate(module, {{count_args, module.typesec[0]}, {count_args, module.typesec[0]}});
    EXPECT_RESULT(execute(*instance_counter, 0, {20, 22}), 2);
    EXPECT_RESULT(execute(*instance_counter, 1, {20}), 1);
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

    constexpr auto host_foo1 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };
    constexpr auto host_foo2 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * args[0]}};
    };

    const auto module = parse(wasm);
    ASSERT_EQ(module.typesec.size(), 2);
    auto instance =
        instantiate(module, {{host_foo1, module.typesec[0]}, {host_foo2, module.typesec[1]}});

    EXPECT_RESULT(execute(*instance, 0, {20, 22}), 42);
    EXPECT_RESULT(execute(*instance, 1, {0x3000'0000}), 0x900'0000'0000'0000);
    EXPECT_RESULT(execute(*instance, 2, {20}), 0x2a002a);
}

TEST(execute, imported_function_traps)
{
    /* wat2wasm
    (import "mod" "foo" (func (param i32 i32) (result i32)))
    */
    const auto wasm = from_hex("0061736d0100000001070160027f7f017f020b01036d6f6403666f6f0000");

    constexpr auto host_foo = [](Instance&, std::vector<uint64_t>) -> execution_result {
        return {true, {}};
    };

    const auto module = parse(wasm);
    auto instance = instantiate(module, {{host_foo, module.typesec[0]}});
    const auto [trap, _] = execute(*instance, 0, {20, 22});
    EXPECT_TRUE(trap);
}

TEST(execute, memory_copy_32bytes)
{
    /* wat2wasm
    (memory 1)

    ;; copy32(dst, src) - copies 4 x 8 bytes using offset immediate.
    (func (param i32 i32)
      get_local 0
      get_local 1
      i64.load offset=0
      i64.store offset=0
      get_local 0
      get_local 1
      i64.load offset=8
      i64.store offset=8
      get_local 0
      get_local 1
      i64.load offset=16
      i64.store offset=16
      get_local 0
      get_local 1
      i64.load offset=24
      i64.store offset=24
    )
    */

    const auto bin = from_hex(
        "0061736d0100000001060160027f7f000302010005030100010a2c012a00200020012903003703002000200129"
        "030837030820002001290310370310200020012903183703180b");

    const auto module = parse(bin);
    auto instance = instantiate(module);
    ASSERT_EQ(instance->memory->size(), 65536);
    const auto input = from_hex("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
    ASSERT_EQ(input.size(), 32);
    std::copy(input.begin(), input.end(), instance->memory->begin());
    const auto [trap, ret] = execute(*instance, 0, {33, 0});
    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
    ASSERT_EQ(instance->memory->size(), 65536);
    bytes output;
    std::copy_n(&(*instance->memory)[33], input.size(), std::back_inserter(output));
    EXPECT_EQ(output, input);
}

TEST(execute, fp_instructions)
{
    /* wat2wasm
    (memory 1)

    ;; FIXME: also check for passing float arguments and use get_local
    (func
      ;; f32 ops
      i32.const 0 ;; mem offset
      f32.const 1
      f32.store
      i32.const 0 ;; mem offset
      f32.load
      f32.const 3.14
      f32.add
      f32.const 1
      f32.sub
      f32.const 3
      f32.mul
      f32.const 2
      f32.div
      f32.const 3
      f32.min
      f32.const 4
      f32.max
      f32.const -1
      f32.copysign
      f32.abs
      f32.neg
      f32.ceil
      f32.floor
      f32.trunc
      f32.nearest
      f32.sqrt
      drop

      f32.const 1
      f32.const 1
      f32.eq
      drop
      f32.const 1
      f32.const 1
      f32.ne
      drop
      f32.const 1
      f32.const 1
      f32.lt
      drop
      f32.const 1
      f32.const 1
      f32.gt
      drop
      f32.const 1
      f32.const 1
      f32.le
      drop
      f32.const 1
      f32.const 1
      f32.ge
      drop

      ;; f64 ops
      i32.const 0 ;; mem offset
      f64.const 1
      f64.store
      i32.const 0 ;; mem offset
      f64.load
      f64.const 3.14
      f64.add
      f64.const 1
      f64.sub
      f64.const 3
      f64.mul
      f64.const 2
      f64.div
      f64.const 3
      f64.min
      f64.const 4
      f64.max
      f64.const -1
      f64.copysign
      f64.abs
      f64.neg
      f64.ceil
      f64.floor
      f64.trunc
      f64.nearest
      f64.sqrt
      drop

      f64.const 1
      f64.const 1
      f64.eq
      drop
      f64.const 1
      f64.const 1
      f64.ne
      drop
      f64.const 1
      f64.const 1
      f64.lt
      drop
      f64.const 1
      f64.const 1
      f64.gt
      drop
      f64.const 1
      f64.const 1
      f64.le
      drop
      f64.const 1
      f64.const 1
      f64.ge
      drop

      ;; conversion ops
      f64.const 1
      f32.demote_f64
      f64.promote_f32
      drop

      f32.const 1
      i32.trunc_f32_s
      drop
      f32.const 1
      i32.trunc_f32_u
      drop
      f64.const 1
      i32.trunc_f64_s
      drop
      f64.const 1
      i32.trunc_f64_u
      drop

      f32.const 1
      i64.trunc_f32_s
      drop
      f32.const 1
      i64.trunc_f32_u
      drop
      f64.const 1
      i64.trunc_f64_s
      drop
      f64.const 1
      i64.trunc_f64_u
      drop

      i32.const 1
      f32.convert_i32_s
      drop
      i32.const 1
      f32.convert_i32_u
      drop
      i64.const 1
      f32.convert_i64_s
      drop
      i64.const 1
      f32.convert_i64_u
      drop

      i32.const 1
      f64.convert_i32_s
      drop
      i32.const 1
      f64.convert_i32_u
      drop
      i64.const 1
      f64.convert_i64_s
      drop
      i64.const 1
      f64.convert_i64_u
      drop

      f32.const 1
      i32.reinterpret_f32
      drop
      f64.const 1
      i64.reinterpret_f64
      drop
      i32.const 1
      f32.reinterpret_i32
      drop
      i64.const 1
      f64.reinterpret_i64
      drop
    )

    (func (param f32 f64)
      unreachable
    )
    */

    const auto bin = from_hex(
        "0061736d0100000001090260000060027d7c00030302000105030100010af90302f203004100430000803f3802"
        "0041002a020043c3f5484092430000803f93430000404094430000004095430000404096430000804097430000"
        "80bf988b8c8d8e8f90911a430000803f430000803f5b1a430000803f430000803f5c1a430000803f430000803f"
        "5d1a430000803f430000803f5e1a430000803f430000803f5f1a430000803f430000803f601a41004400000000"
        "0000f03f39030041002b0300441f85eb51b81e0940a044000000000000f03fa1440000000000000840a2440000"
        "000000000040a3440000000000000840a4440000000000001040a544000000000000f0bfa6999a9b9c9d9e9f1a"
        "44000000000000f03f44000000000000f03f611a44000000000000f03f44000000000000f03f621a4400000000"
        "0000f03f44000000000000f03f631a44000000000000f03f44000000000000f03f641a44000000000000f03f44"
        "000000000000f03f651a44000000000000f03f44000000000000f03f661a44000000000000f03fb6bb1a430000"
        "803fa81a430000803fa91a44000000000000f03faa1a44000000000000f03fab1a430000803fae1a430000803f"
        "af1a44000000000000f03fb01a44000000000000f03fb11a4101b21a4101b31a4201b41a4201b51a4101b71a41"
        "01b81a4201b91a4201ba1a430000803fbc1a44000000000000f03fbd1a4101be1a4201bf1a0b0300000b");

    const auto module = parse(bin);
    auto instance = instantiate(module);

    // First function with floating point instructions.
    EXPECT_THROW_MESSAGE(
        execute(*instance, 0, {}), unsupported_feature, "Floating point instruction.");

    // Second function with floating point parameters.
    ASSERT_TRUE(execute(*instance, 1, {}).trapped);
}
