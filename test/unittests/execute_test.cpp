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
    module.codesec.emplace_back(Code{0, {Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, call)
{
    Module module;
    module.typesec.emplace_back(FuncType{{}, {ValType::i32}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(Code{0, {Instr::i32_const, Instr::end}, {42, 0, 42, 0}});
    module.codesec.emplace_back(Code{0, {Instr::call, Instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 1, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0x2a002a);
}

TEST(execute, call_trap)
{
    Module module;
    module.typesec.emplace_back(FuncType{{}, {ValType::i32}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(Code{0, {Instr::unreachable, Instr::end}, {}});
    module.codesec.emplace_back(Code{0, {Instr::call, Instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 1, {});

    ASSERT_TRUE(trap);
}

TEST(execute, call_with_arguments)
{
    /* wat2wasm
    (module
      (func $calc (param $a i32) (param $b i32) (result i32)
        local.get 1
        local.get 0
        i32.sub ;; a - b
      )
      (func (result i32)
        i32.const 13
        i32.const 17
        call $calc ;; 17 - 13 => 4
      )
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010b0260027f7f017f6000017f03030200010a12020700200120006b0b0800410d41111000"
        "0b");

    EXPECT_RESULT(execute(parse(wasm), 1, {}), 4);
}

TEST(execute, call_indirect)
{
    /* wat2wasm
      (type $out-i32 (func (result i32)))

      (table anyfunc (elem $f3 $f2 $f1 $f4 $f5))

      (func $f1 (result i32) i32.const 1)
      (func $f2 (result i32) i32.const 2)
      (func $f3 (result i32) i32.const 3)
      (func $f4 (result i64) i64.const 4)
      (func $f5 (result i32) unreachable)

      (func (param i32) (result i32)
        (call_indirect (type $out-i32) (get_local 0))
      )
    */
    const auto bin = from_hex(
        "0061736d01000000010e036000017f6000017e60017f017f03070600000001000204050170010505090b010041"
        "000b0502010003040a2106040041010b040041020b040041030b040042040b0300000b070020001100000b");

    const Module module = parse(bin);

    for (const auto param : {0u, 1u, 2u})
    {
        constexpr uint64_t expected_results[]{3, 2, 1};

        const auto [trap, ret] = execute(module, 5, {param});
        ASSERT_FALSE(trap);
        ASSERT_EQ(ret.size(), 1);
        EXPECT_EQ(ret[0], expected_results[param]);
    }

    // immediate is incorrect type
    EXPECT_TRUE(execute(module, 5, {3}).trapped);

    // called function traps
    EXPECT_TRUE(execute(module, 5, {4}).trapped);

    // argument out of table bounds
    EXPECT_TRUE(execute(module, 5, {5}).trapped);
}

TEST(execute, call_indirect_with_argument)
{
    /* wat2wasm
    (module
      (type $bin_func (func (param i32 i32) (result i32)))
      (table anyfunc (elem $f1 $f2 $f3))

      (func $f1 (param i32 i32) (result i32) (i32.div_u (get_local 0) (get_local 1)))
      (func $f2 (param i32 i32) (result i32) (i32.sub (get_local 0) (get_local 1)))
      (func $f3 (param i32) (result i32) (i32.mul (get_local 0) (get_local 0)))

      (func (param i32) (result i32)
        i32.const 31
        i32.const 7
        (call_indirect (type $bin_func) (get_local 0))
      )
    )
    */
    const auto bin = from_hex(
        "0061736d01000000010c0260027f7f017f60017f017f03050400000101040501700103030909010041000b0300"
        "01020a25040700200020016e0b0700200020016b0b0700200020006c0b0b00411f410720001100000b");

    const Module module = parse(bin);

    EXPECT_RESULT(execute(module, 3, {0}), 31 / 7);
    EXPECT_RESULT(execute(module, 3, {1}), 31 - 7);

    // immediate is incorrect type
    EXPECT_TRUE(execute(module, 3, {2}).trapped);
}

TEST(execute, call_indirect_imported_table)
{
    /* wat2wasm
    (module
      (type $out_i32 (func (result i32)))
      (import "m" "t" (table 5 20 anyfunc))

      (func $f1 (result i32) i32.const 1)
      (func $f2 (result i32) i32.const 2)
      (func $f3 (result i32) i32.const 3)
      (func $f4 (result i64) i64.const 4)
      (func $f5 (result i32) unreachable)

      (func (param i32) (result i32)
        (call_indirect (type $out_i32) (get_local 0))
      )
    )
    */
    const auto bin = from_hex(
        "0061736d01000000010e036000017f6000017e60017f017f020a01016d01740170010514030706000000010002"
        "0a2106040041010b040041020b040041030b040042040b0300000b070020001100000b");

    const Module module = parse(bin);

    std::vector<FuncIdx> table{2, 1, 0, 3, 4};
    auto instance = instantiate(module, {}, {{&table, {5, 20}}});

    for (const auto param : {0u, 1u, 2u})
    {
        constexpr uint64_t expected_results[]{3, 2, 1};

        const auto [trap, ret] = execute(instance, 5, {param});
        ASSERT_FALSE(trap);
        ASSERT_EQ(ret.size(), 1);
        EXPECT_EQ(ret[0], expected_results[param]);
    }

    // immediate is incorrect type
    EXPECT_TRUE(execute(instance, 5, {3}).trapped);

    // called function traps
    EXPECT_TRUE(execute(instance, 5, {4}).trapped);

    // argument out of table bounds
    EXPECT_TRUE(execute(instance, 5, {5}).trapped);
}

TEST(execute, drop)
{
    Module module;
    module.codesec.emplace_back(Code{1, {Instr::local_get, Instr::drop, Instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
}

TEST(execute, select)
{
    Module module;
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
    module.codesec.emplace_back(Code{0, {Instr::local_get, Instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = execute(module, 0, {42});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_set)
{
    Module module;
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
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});

    module.codesec.emplace_back(Code{0, {Instr::global_get, Instr::end}, {0, 0, 0, 0}});

    auto instance = instantiate(module);

    const auto [trap, ret] = execute(instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, global_get_two_globals)
{
    Module module;
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {43}}});

    module.codesec.emplace_back(Code{0, {Instr::global_get, Instr::end}, {0, 0, 0, 0}});
    module.codesec.emplace_back(Code{0, {Instr::global_get, Instr::end}, {1, 0, 0, 0}});

    auto instance = instantiate(module);

    auto [trap, ret] = execute(instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);

    auto [trap2, ret2] = execute(instance, 1, {});

    ASSERT_FALSE(trap2);
    ASSERT_EQ(ret2.size(), 1);
    EXPECT_EQ(ret2[0], 43);
}

TEST(execute, global_get_imported)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "glob", ExternalKind::Global, {false}});
    module.codesec.emplace_back(Code{0, {Instr::global_get, Instr::end}, {0, 0, 0, 0}});

    uint64_t global_value = 42;
    auto instance = instantiate(module, {}, {}, {}, {ExternalGlobal{&global_value, false}});

    const auto [trap, ret] = execute(instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);

    global_value = 43;

    const auto [trap2, ret2] = execute(instance, 0, {});

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

    EXPECT_RESULT(execute(instance, 0, {}), 40);
    EXPECT_RESULT(execute(instance, 1, {}), 41);
    EXPECT_RESULT(execute(instance, 2, {}), 42);
    EXPECT_RESULT(execute(instance, 3, {}), 43);
}

TEST(execute, global_set)
{
    Module module;
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {41}}});

    module.codesec.emplace_back(
        Code{0, {Instr::i32_const, Instr::global_set, Instr::end}, {42, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);

    const auto [trap, ret] = execute(instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(instance.globals[0], 42);
}

TEST(execute, global_set_two_globals)
{
    Module module;
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {43}}});

    module.codesec.emplace_back(Code{0,
        {Instr::i32_const, Instr::global_set, Instr::i32_const, Instr::global_set, Instr::end},
        {44, 0, 0, 0, 0, 0, 0, 0, 45, 0, 0, 0, 1, 0, 0, 0}});

    auto instance = instantiate(module);

    const auto [trap, ret] = execute(instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(instance.globals[0], 44);
    ASSERT_EQ(instance.globals[1], 45);
}

TEST(execute, global_set_imported)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "glob", ExternalKind::Global, {true}});
    module.codesec.emplace_back(
        Code{0, {Instr::i32_const, Instr::global_set, Instr::end}, {42, 0, 0, 0, 0, 0, 0, 0}});

    uint64_t global_value = 41;
    auto instance = instantiate(module, {}, {}, {}, {ExternalGlobal{&global_value, true}});

    const auto [trap, ret] = execute(instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(global_value, 42);
}

TEST(execute, i32_load)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 42;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 42);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i32_load_imported_memory)
{
    Module module;
    Import imp{"mod", "m", ExternalKind::Memory, {}};
    imp.desc.memory = Memory{{1, 1}};
    module.importsec.emplace_back(imp);
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, 1}}});
    memory[0] = 42;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 42);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i64_load)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 0x2a;
    (*instance.memory)[4] = 0x2a;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 0x2a0000002a);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i32_load8_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load8_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 0x80;
    (*instance.memory)[1] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(static_cast<int32_t>(ret[0]), -128);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i32_load8_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load8_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 0x81;
    (*instance.memory)[1] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0x01);
    ASSERT_EQ(static_cast<uint32_t>(ret[0]), 129);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i32_load16_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load16_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 0x00;
    (*instance.memory)[1] = 0x80;
    (*instance.memory)[3] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(static_cast<int32_t>(ret[0]), -32768);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i32_load16_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i32_load16_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 0x01;
    (*instance.memory)[1] = 0x80;
    (*instance.memory)[3] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0x01);
    ASSERT_EQ(static_cast<uint32_t>(ret[0]), 32769);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i64_load8_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load8_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 0x80;
    (*instance.memory)[1] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], uint64_t(-128));

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i64_load8_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load8_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 0x81;
    (*instance.memory)[1] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 0x81);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i64_load16_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load16_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 0x00;
    (*instance.memory)[1] = 0x80;
    (*instance.memory)[2] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], uint64_t(-32768));

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i64_load16_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load16_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    (*instance.memory)[0] = 0x01;
    (*instance.memory)[1] = 0x80;
    (*instance.memory)[2] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 0x8001);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i64_load32_s)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load32_s, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    auto& memory = *instance.memory;
    memory[0] = 0x00;
    memory[1] = 0x00;
    memory[2] = 0x00;
    memory[3] = 0x80;
    memory[4] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], uint64_t(-2147483648));

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i64_load32_u)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::i64_load32_u, Instr::end}, {0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    auto& memory = *instance.memory;
    memory[0] = 0x01;
    memory[1] = 0x00;
    memory[2] = 0x00;
    memory[3] = 0x80;
    memory[4] = 0xf1;
    const auto [trap, ret] = execute(instance, 0, {0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    ASSERT_EQ(ret[0], 0x80000001);

    ASSERT_TRUE(execute(instance, 0, {65537}).trapped);
}

TEST(execute, i32_store)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(instance, 0, {42, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance.memory->substr(0, 4), from_hex("2a000000"));

    ASSERT_TRUE(execute(instance, 0, {42, 65537}).trapped);
}

TEST(execute, i32_store_imported_memory)
{
    Module module;
    Import imp{"mod", "m", ExternalKind::Memory, {}};
    imp.desc.memory = Memory{{1, 1}};
    module.importsec.emplace_back(imp);
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, 1}}});
    const auto [trap, ret] = execute(instance, 0, {42, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(memory.substr(0, 4), from_hex("2a000000"));

    ASSERT_TRUE(execute(instance, 0, {42, 65537}).trapped);
}

TEST(execute, i64_store)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i64_store, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(instance, 0, {0x2a0000002a, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance.memory->substr(0, 8), from_hex("2a0000002a000000"));

    ASSERT_TRUE(execute(instance, 0, {0x2a0000002a, 65537}).trapped);
}

TEST(execute, i32_store8)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store8, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(instance, 0, {0xf1f2f380, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance.memory->substr(0, 4), from_hex("80000000"));

    ASSERT_TRUE(execute(instance, 0, {0xf1f2f380, 65537}).trapped);
}

TEST(execute, i32_store8_trap)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store8, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(instance, 0, {0xf1f2f380, 65537});

    ASSERT_TRUE(trap);
}

TEST(execute, i32_store16)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i32_store16, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(instance, 0, {0xf1f28000, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance.memory->substr(0, 4), from_hex("00800000"));

    ASSERT_TRUE(execute(instance, 0, {0xf1f28000, 65537}).trapped);
}

TEST(execute, i64_store8)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i64_store8, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(instance, 0, {0xf1f2f4f5f6f7f880, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance.memory->substr(0, 8), from_hex("8000000000000000"));

    ASSERT_TRUE(execute(instance, 0, {0xf1f2f4f5f6f7f880, 65537}).trapped);
}

TEST(execute, i64_store16)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i64_store16, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(instance, 0, {0xf1f2f4f5f6f78000, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance.memory->substr(0, 8), from_hex("0080000000000000"));

    ASSERT_TRUE(execute(instance, 0, {0xf1f2f4f5f6f78000, 65537}).trapped);
}

TEST(execute, i64_store32)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::local_get, Instr::i64_store32, Instr::end},
            {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto instance = instantiate(module);
    const auto [trap, ret] = execute(instance, 0, {0xf1f2f4f580000000, 0});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 0);
    ASSERT_EQ(instance.memory->substr(0, 8), from_hex("0000008000000000"));

    ASSERT_TRUE(execute(instance, 0, {0xf1f2f4f580000000, 65537}).trapped);
}

TEST(execute, memory_size)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.codesec.emplace_back(Code{0, {Instr::memory_size, Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
}

TEST(execute, memory_grow)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 4096}});
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
    ASSERT_EQ(instance.memory->substr(0, 4), from_hex("2a000000"));

    const auto [trap, ret] = execute(instance, 0, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
    EXPECT_EQ(instance.memory->substr(0, 4), from_hex("2a000000"));
}

TEST(execute, imported_function)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32, ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {0}});

    auto host_foo = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };

    auto instance = instantiate(module, {host_foo});

    const auto [trap, ret] = execute(instance, 0, {20, 22});

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

    auto instance = instantiate(module, {host_foo1, host_foo2});

    const auto [trap1, ret1] = execute(instance, 0, {20, 22});

    ASSERT_FALSE(trap1);
    ASSERT_EQ(ret1.size(), 1);
    EXPECT_EQ(ret1[0], 42);

    const auto [trap2, ret2] = execute(instance, 1, {20, 22});

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
    module.codesec.emplace_back(Code{0, {Instr::i32_const, Instr::end}, {42, 0, 42, 0}});

    auto host_foo1 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };
    auto host_foo2 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * args[0]}};
    };

    auto instance = instantiate(module, {host_foo1, host_foo2});

    const auto [trap1, ret1] = execute(instance, 0, {20, 22});

    ASSERT_FALSE(trap1);
    ASSERT_EQ(ret1.size(), 1);
    EXPECT_EQ(ret1[0], 42);

    const auto [trap2, ret2] = execute(instance, 1, {20});

    ASSERT_FALSE(trap2);
    ASSERT_EQ(ret2.size(), 1);
    EXPECT_EQ(ret2[0], 400);

    // check correct number of arguments is passed to host
    auto count_args = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args.size()}};
    };

    auto instance_couner = instantiate(module, {count_args, count_args});

    const auto [trap3, ret3] = execute(instance_couner, 0, {20, 22});

    ASSERT_FALSE(trap3);
    ASSERT_EQ(ret3.size(), 1);
    EXPECT_EQ(ret3[0], 2);

    const auto [trap4, ret4] = execute(instance_couner, 1, {20});

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
    module.codesec.emplace_back(Code{0, {Instr::i32_const, Instr::end}, {42, 0, 42, 0}});

    auto host_foo1 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] + args[1]}};
    };
    auto host_foo2 = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * args[0]}};
    };

    auto instance = instantiate(module, {host_foo1, host_foo2});

    const auto [trap1, ret1] = execute(instance, 0, {20, 22});

    ASSERT_FALSE(trap1);
    ASSERT_EQ(ret1.size(), 1);
    EXPECT_EQ(ret1[0], 42);

    const auto [trap2, ret2] = execute(instance, 1, {20});

    ASSERT_FALSE(trap2);
    ASSERT_EQ(ret2.size(), 1);
    EXPECT_EQ(ret2[0], 400);

    const auto [trap3, ret3] = execute(instance, 2, {20});

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

    auto instance = instantiate(module, {host_foo});

    const auto [trap, ret] = execute(instance, 0, {20, 22});

    ASSERT_TRUE(trap);
}

TEST(execute, imported_function_call)
{
    Module module;
    module.typesec.emplace_back(FuncType{{}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {0}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(Code{0, {Instr::call, Instr::end}, {0, 0, 0, 0}});

    auto host_foo = [](Instance&, std::vector<uint64_t>) -> execution_result {
        return {false, {42}};
    };

    auto instance = instantiate(module, {host_foo});

    const auto [trap, ret] = execute(instance, 1, {});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, imported_function_call_with_arguments)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {0}});
    module.funcsec.emplace_back(TypeIdx{0});
    module.codesec.emplace_back(
        Code{0, {Instr::local_get, Instr::call, Instr::i32_const, Instr::i32_add, Instr::end},
            {0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0}});

    auto host_foo = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * 2}};
    };

    auto instance = instantiate(module, {host_foo});

    const auto [trap, ret] = execute(instance, 1, {20});

    ASSERT_FALSE(trap);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, imported_functions_call_indirect)
{
    /* wat2wasm
    (module
      (type $ft (func (param i32) (result i64)))
      (func $sqr    (import "env" "sqr") (param i32) (result i64))
      (func $isqrt  (import "env" "isqrt") (param i32) (result i64))
      (func $double (param i32) (result i64)
        get_local 0
        i64.extend_u/i32
        get_local 0
        i64.extend_u/i32
        i64.add
      )

      (func $main (param i32) (param i32) (result i64)
        get_local 1
        get_local 0
        call_indirect (type $ft)
      )

      (table anyfunc (elem $double $sqr $isqrt))
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010c0260017f017e60027f7f017e02170203656e7603737172000003656e76056973717274"
        "00000303020001040501700103030909010041000b030200010a150209002000ad2000ad7c0b09002001200011"
        "00000b");

    const auto module = parse(wasm);
    ASSERT_EQ(module.typesec.size(), 2);
    ASSERT_EQ(module.importsec.size(), 2);
    ASSERT_EQ(module.codesec.size(), 2);

    constexpr auto sqr = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {args[0] * args[0]}};
    };
    constexpr auto isqrt = [](Instance&, std::vector<uint64_t> args) -> execution_result {
        return {false, {(11 + args[0] / 11) / 2}};
    };

    auto instance = instantiate(module, {sqr, isqrt});
    EXPECT_RESULT(execute(instance, 3, {0, 10}), 20);  // double(10)
    EXPECT_RESULT(execute(instance, 3, {1, 9}), 81);   // sqr(9)
    EXPECT_RESULT(execute(instance, 3, {2, 50}), 7);   // isqrt(50)
}

TEST(execute, imported_function_from_another_module)
{
    /* wat2wasm
    (module
      (func $sub (param $lhs i32) (param $rhs i32) (result i32)
        get_local $lhs
        get_local $rhs
        i32.sub)
      (export "sub" (func $sub))
    )
    */
    const auto bin1 = from_hex(
        "0061736d0100000001070160027f7f017f030201000707010373756200000a09010700200020016b0b");
    const auto module1 = parse(bin1);
    auto instance1 = instantiate(module1);

    /* wat2wasm
    (module
      (func $sub (import "m1" "sub") (param $lhs i32) (param $rhs i32) (result i32))

      (func $main (param i32) (param i32) (result i32)
        get_local 0
        get_local 1
        call $sub
      )
    )
    */
    const auto bin2 = from_hex(
        "0061736d0100000001070160027f7f017f020a01026d31037375620000030201000a0a0108002000200110000"
        "b");
    const auto module2 = parse(bin2);

    const auto func_idx = fizzy::find_exported_function(module1, "sub");
    ASSERT_TRUE(func_idx.has_value());

    auto sub = [&instance1, func_idx](Instance&, std::vector<uint64_t> args) -> execution_result {
        return fizzy::execute(instance1, *func_idx, std::move(args));
    };

    auto instance2 = instantiate(module2, {sub});

    EXPECT_RESULT(execute(instance2, 1, {44, 2}), 42);
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
    ASSERT_EQ(instance.memory->size(), 65536);
    const auto input = from_hex("0102030405060708090a0b0c0d0e0f101112131415161718191a1b1c1d1e1f20");
    ASSERT_EQ(input.size(), 32);
    std::copy(input.begin(), input.end(), instance.memory->begin());
    const auto [trap, ret] = execute(instance, 0, {33, 0});
    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
    ASSERT_EQ(instance.memory->size(), 65536);
    bytes output;
    std::copy_n(&(*instance.memory)[33], input.size(), std::back_inserter(output));
    EXPECT_EQ(output, input);
}
