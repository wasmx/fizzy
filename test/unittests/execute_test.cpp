#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;

TEST(execute, end)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(Code{0, {Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, drop)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(Code{1, {Instr::local_get, Instr::drop, Instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
}

TEST(execute, select)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::local_get, Instr::select, Instr::end},
            {0, 0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0}});

    auto result = execute(module, 0, {3, 6, 0});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 6);

    result = execute(module, 0, {3, 6, 1});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 3);

    result = execute(module, 0, {3, 6, 42});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 3);
}

TEST(execute, local_get)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(Code{0, {Instr::local_get, Instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_set)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{1, {Instr::local_get, Instr::local_set, Instr::local_get, Instr::end},
            {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_tee)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{1, {Instr::local_get, Instr::local_tee, Instr::end}, {0, 0, 0, 0, 1, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, global_get)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module.codesec.emplace_back(Code{0, {Instr::global_get, Instr::end}, {0, 0, 0, 0}});

    auto instance = instantiate(module);

    const auto [trap, ret] = execute(*instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, global_get_two_globals)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.funcsec.emplace_back(TypeIdx{0});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {43}}});
    module.codesec.emplace_back(Code{0, {Instr::global_get, Instr::end}, {0, 0, 0, 0}});
    module.codesec.emplace_back(Code{0, {Instr::global_get, Instr::end}, {1, 0, 0, 0}});

    auto instance = instantiate(module);

    auto [trap, ret] = execute(*instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);

    auto [trap2, ret2] = execute(*instance, 1, {});

    ASSERT_FALSE(trap2);
    ASSERT_EQ(ret2.size(), 1);
    EXPECT_EQ(ret2[0], 43);
}

TEST(execute, global_get_imported)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.importsec.emplace_back(Import{"mod", "glob", ExternalKind::Global, {false}});
    module.codesec.emplace_back(Code{0, {Instr::global_get, Instr::end}, {0, 0, 0, 0}});

    uint64_t global_value = 42;
    auto instance = instantiate(module, {}, {}, {}, {ExternalGlobal{&global_value, false}});

    const auto [trap, ret] = execute(*instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);

    global_value = 43;

    const auto [trap2, ret2] = execute(*instance, 0, {});

    ASSERT_FALSE(trap2);
    ASSERT_EQ(ret2.size(), 1);
    EXPECT_EQ(ret2[0], 43);
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
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {41}}});
    module.codesec.emplace_back(
        Code{0, {Instr::i32_const, Instr::global_set, Instr::end}, {42, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);

    const auto [trap, ret] = execute(*instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(instance->globals[0], 42);
}

TEST(execute, global_set_two_globals)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {43}}});
    module.codesec.emplace_back(Code{0,
        {Instr::i32_const, Instr::global_set, Instr::i32_const, Instr::global_set, Instr::end},
        {44, 0, 0, 0, 0, 0, 0, 0, 45, 0, 0, 0, 1, 0, 0, 0}});

    auto instance = instantiate(module);

    const auto [trap, ret] = execute(*instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(instance->globals[0], 44);
    ASSERT_EQ(instance->globals[1], 45);
}

TEST(execute, global_set_imported)
{
    Module module;
    module.funcsec.emplace_back(TypeIdx{0});
    module.importsec.emplace_back(Import{"mod", "glob", ExternalKind::Global, {true}});
    module.codesec.emplace_back(
        Code{0, {Instr::i32_const, Instr::global_set, Instr::end}, {42, 0, 0, 0, 0, 0, 0, 0}});

    uint64_t global_value = 41;
    auto instance = instantiate(module, {}, {}, {}, {ExternalGlobal{&global_value, true}});

    const auto [trap, ret] = execute(*instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(global_value, 42);
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
    Module module;
    Import imp{"mod", "m", ExternalKind::Memory, {}};
    imp.desc.memory = Memory{{1, 1}};
    module.funcsec.emplace_back(TypeIdx{0});
    module.importsec.emplace_back(imp);
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, 1}}});
    memory[0] = 42;
    const auto [trap, ret] = execute(*instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 42);

    ASSERT_TRUE(execute(*instance, 0, {65537}).trapped);
}

TEST(execute, i32_load_overflow)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    // NOTE: this is i32.load offset=0x7fffffff
    module.codesec.emplace_back(Code{
        0, {Instr::local_get, Instr::i32_load, Instr::end}, {0, 0, 0, 0, 0xff, 0xff, 0xff, 0x7f}});

    auto instance = instantiate(module);

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
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    // NOTE: this is i64.load offset=0x7fffffff
    module.codesec.emplace_back(Code{
        0, {Instr::local_get, Instr::i64_load, Instr::end}, {0, 0, 0, 0, 0xff, 0xff, 0xff, 0x7f}});

    auto instance = instantiate(module);

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

TEST(execute, i32_store)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(*instance, 0, {42, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance->memory->substr(0, 4), from_hex("2a000000"));

    ASSERT_TRUE(execute(*instance, 0, {42, 65537}).trapped);
}

TEST(execute, i32_store_imported_memory)
{
    Module module;
    Import imp{"mod", "m", ExternalKind::Memory, {}};
    imp.desc.memory = Memory{{1, 1}};
    module.importsec.emplace_back(imp);
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, 1}}});
    const auto [trap, ret] = execute(*instance, 0, {42, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(memory.substr(0, 4), from_hex("2a000000"));

    ASSERT_TRUE(execute(*instance, 0, {42, 65537}).trapped);
}

TEST(execute, i32_store_overflow)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    // NOTE: this is i32.store offset=0x7fffffff
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_const, Instr::i32_store, Instr::end},
            {0, 0, 0, 0, 0x55, 0xaa, 0x55, 0xaa, 0xff, 0xff, 0xff, 0x7f}});

    auto instance = instantiate(module);

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    ASSERT_TRUE(execute(*instance, 0, {0}).trapped);
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    ASSERT_TRUE(execute(*instance, 0, {0x80000000}).trapped);
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    ASSERT_TRUE(execute(*instance, 0, {0x80000001}).trapped);
}

TEST(execute, i32_store_no_memory)
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

TEST(execute, f32_store_no_memory)
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

TEST(execute, memory_size_no_memory)
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

TEST(execute, i64_store)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i64_store, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(*instance, 0, {0x2a0000002a, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance->memory->substr(0, 8), from_hex("2a0000002a000000"));

    ASSERT_TRUE(execute(*instance, 0, {0x2a0000002a, 65537}).trapped);
}

TEST(execute, i64_store_overflow)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    // NOTE: this is i64.store offset=0x7fffffff
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_const, Instr::i64_store, Instr::end},
            {0, 0, 0, 0, 0x55, 0xaa, 0x55, 0xaa, 0xff, 0xff, 0xff, 0x7f}});

    auto instance = instantiate(module);

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    ASSERT_TRUE(execute(*instance, 0, {0}).trapped);
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    ASSERT_TRUE(execute(*instance, 0, {0x80000000}).trapped);
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    ASSERT_TRUE(execute(*instance, 0, {0x80000001}).trapped);
}

TEST(execute, i32_store8)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store8, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(*instance, 0, {0xf1f2f380, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance->memory->substr(0, 4), from_hex("80000000"));

    ASSERT_TRUE(execute(*instance, 0, {0xf1f2f380, 65537}).trapped);
}

TEST(execute, i32_store8_trap)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store8, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(*instance, 0, {0xf1f2f380, 65537});

    ASSERT_TRUE(trap);
}

TEST(execute, i32_store16)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store16, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(*instance, 0, {0xf1f28000, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance->memory->substr(0, 4), from_hex("00800000"));

    ASSERT_TRUE(execute(*instance, 0, {0xf1f28000, 65537}).trapped);
}

TEST(execute, i64_store8)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i64_store8, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(*instance, 0, {0xf1f2f4f5f6f7f880, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance->memory->substr(0, 8), from_hex("8000000000000000"));

    ASSERT_TRUE(execute(*instance, 0, {0xf1f2f4f5f6f7f880, 65537}).trapped);
}

TEST(execute, i64_store16)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i64_store16, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(*instance, 0, {0xf1f2f4f5f6f78000, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance->memory->substr(0, 8), from_hex("0080000000000000"));

    ASSERT_TRUE(execute(*instance, 0, {0xf1f2f4f5f6f78000, 65537}).trapped);
}

TEST(execute, i64_store32)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i64_store32, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(*instance, 0, {0xf1f2f4f580000000, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance->memory->substr(0, 8), from_hex("0000008000000000"));

    ASSERT_TRUE(execute(*instance, 0, {0xf1f2f4f580000000, 65537}).trapped);
}

TEST(execute, memory_size)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(Code{0, {Instr::memory_size, Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
}

TEST(execute, memory_grow)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 4096}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::memory_grow, Instr::end}, {0, 0, 0, 0}});

    auto result = execute(module, 0, {0});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    result = execute(module, 0, {1});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    // 256MB memory.
    result = execute(module, 0, {4095});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], 1);

    // >256MB memory.
    result = execute(module, 0, {4096});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], uint32_t(-1));

    // Way too high (but still within bounds)
    result = execute(module, 0, {0xffffffe});

    ASSERT_FALSE(result.trapped);
    ASSERT_EQ(result.stack.size(), 1);
    EXPECT_EQ(result.stack[0], uint32_t(-1));
}

TEST(execute, start_section)
{
    // In this test the start function (index 1) writes a i32 value to the memory
    // and the same is read back in the "main" function (index 0).
    Module module;
    module.startfunc = FuncIdx{1};
    module.memorysec.emplace_back(Memory{{1, 1}});
    // TODO: add type section (once enforced)
    module.funcsec.emplace_back(FuncIdx{0});
    module.funcsec.emplace_back(FuncIdx{1});
    module.codesec.emplace_back(
        Code{0, {Instr::i32_const, Instr::i32_load, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});
    module.codesec.emplace_back(
        Code{0, {Instr::i32_const, Instr::i32_const, Instr::i32_store, Instr::end},
            {0, 0, 0, 0, 42, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    // Start function sets this
    ASSERT_EQ(instance->memory->substr(0, 4), from_hex("2a000000"));

    const auto [trap, ret] = execute(*instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
    EXPECT_EQ(instance->memory->substr(0, 4), from_hex("2a000000"));
}

TEST(execute, imported_function)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32, ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {0}});

    auto host_foo = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };

    auto instance = instantiate(module, {{host_foo, module.typesec[0]}});

    const auto [trap, ret] = execute(*instance, 0, {20, 22});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, imported_two_functions)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32, ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo1", ExternalKind::Function, {0}});
    module.importsec.emplace_back(Import{"mod", "foo2", ExternalKind::Function, {0}});

    auto host_foo1 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };
    auto host_foo2 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * args[1]}};
    };

    auto instance =
        instantiate(module, {{host_foo1, module.typesec[0]}, {host_foo2, module.typesec[0]}});

    const auto [trap1, ret1] = execute(*instance, 0, {20, 22});

    ASSERT_FALSE(trap1);
    ASSERT_EQ(ret1.size(), 1);
    EXPECT_EQ(ret1[0], 42);

    const auto [trap2, ret2] = execute(*instance, 1, {20, 22});

    ASSERT_FALSE(trap2);
    ASSERT_EQ(ret2.size(), 1);
    EXPECT_EQ(ret2[0], 440);
}

TEST(execute, imported_functions_and_regular_one)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32, ValType::i32}, {ValType::i32}});
    module.typesec.emplace_back(FuncType{{ValType::i64}, {ValType::i64}});
    module.importsec.emplace_back(Import{"mod", "foo1", ExternalKind::Function, {0}});
    module.importsec.emplace_back(Import{"mod", "foo2", ExternalKind::Function, {0}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(Code{0, {Instr::i32_const, Instr::end}, {42, 0, 42, 0}});

    auto host_foo1 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };
    auto host_foo2 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * args[0]}};
    };

    auto instance =
        instantiate(module, {{host_foo1, module.typesec[0]}, {host_foo2, module.typesec[0]}});

    const auto [trap1, ret1] = execute(*instance, 0, {20, 22});

    ASSERT_FALSE(trap1);
    ASSERT_EQ(ret1.size(), 1);
    EXPECT_EQ(ret1[0], 42);

    const auto [trap2, ret2] = execute(*instance, 1, {20});

    ASSERT_FALSE(trap2);
    ASSERT_EQ(ret2.size(), 1);
    EXPECT_EQ(ret2[0], 400);

    // check correct number of arguments is passed to host
    auto count_args = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args.size()}};
    };

    auto instance_couner =
        instantiate(module, {{count_args, module.typesec[0]}, {count_args, module.typesec[0]}});

    const auto [trap3, ret3] = execute(*instance_couner, 0, {20, 22});

    ASSERT_FALSE(trap3);
    ASSERT_EQ(ret3.size(), 1);
    EXPECT_EQ(ret3[0], 2);

    const auto [trap4, ret4] = execute(*instance_couner, 1, {20});

    ASSERT_FALSE(trap4);
    ASSERT_EQ(ret4.size(), 1);
    EXPECT_EQ(ret4[0], 1);
}

TEST(execute, imported_two_functions_different_type)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32, ValType::i32}, {ValType::i32}});
    module.typesec.emplace_back(FuncType{{ValType::i64}, {ValType::i64}});
    module.importsec.emplace_back(Import{"mod", "foo1", ExternalKind::Function, {0}});
    module.importsec.emplace_back(Import{"mod", "foo2", ExternalKind::Function, {0}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(Code{0, {Instr::i32_const, Instr::end}, {42, 0, 42, 0}});

    auto host_foo1 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };
    auto host_foo2 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * args[0]}};
    };

    auto instance =
        instantiate(module, {{host_foo1, module.typesec[0]}, {host_foo2, module.typesec[0]}});

    const auto [trap1, ret1] = execute(*instance, 0, {20, 22});

    ASSERT_FALSE(trap1);
    ASSERT_EQ(ret1.size(), 1);
    EXPECT_EQ(ret1[0], 42);

    const auto [trap2, ret2] = execute(*instance, 1, {20});

    ASSERT_FALSE(trap2);
    ASSERT_EQ(ret2.size(), 1);
    EXPECT_EQ(ret2[0], 400);

    const auto [trap3, ret3] = execute(*instance, 2, {20});

    ASSERT_FALSE(trap3);
    ASSERT_EQ(ret3.size(), 1);
    EXPECT_EQ(ret3[0], 0x2a002a);
}

TEST(execute, imported_function_traps)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32, ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {0}});

    auto host_foo = [](Instance&, std::vector<uint64_t>) -> execution_result { return {true, {}}; };

    auto instance = instantiate(module, {{host_foo, module.typesec[0]}});

    const auto [trap, ret] = execute(*instance, 0, {20, 22});

    ASSERT_TRUE(trap);
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
