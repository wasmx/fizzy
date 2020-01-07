#include "execute.hpp"
#include "parser.hpp"
#include "utils.hpp"
#include <gtest/gtest.h>

TEST(execute, end)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0, {fizzy::instr::end}, {}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_EQ(trap, false);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, unreachable)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0, {fizzy::instr::unreachable, fizzy::instr::end}, {}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_EQ(trap, true);
}

TEST(execute, nop)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0, {fizzy::instr::nop, fizzy::instr::end}, {}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_EQ(trap, false);
    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, local_get)
{
    fizzy::module module;
    module.codesec.emplace_back(
        fizzy::code{0, {fizzy::instr::local_get, fizzy::instr::end}, {0, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {42});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_set)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{1,
        {fizzy::instr::local_get, fizzy::instr::local_set, fizzy::instr::local_get,
            fizzy::instr::end},
        {0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {42});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_tee)
{
    fizzy::module module;
    module.codesec.emplace_back(
        fizzy::code{1, {fizzy::instr::local_get, fizzy::instr::local_tee, fizzy::instr::end},
            {0, 0, 0, 0, 1, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {42});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, basic_add)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{1,
        {fizzy::instr::local_get, fizzy::instr::local_get, fizzy::instr::i32_add,
            fizzy::instr::end},
        {0, 0, 0, 0, 0, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 0);
}

TEST(execute, basic_add_with_inputs)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0,
        {fizzy::instr::local_get, fizzy::instr::local_get, fizzy::instr::i32_add,
            fizzy::instr::end},
        {0, 0, 0, 0, 1, 0, 0, 0}});

    const auto [trap, ret] = fizzy::execute(module, 0, {20, 22});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, milestone1)
{
    /*
    (module
      (func $add (param $lhs i32) (param $rhs i32) (result i32)
        (local $local1 i32)
        local.get $lhs
        local.get $rhs
        i32.add
        local.get $local1
        i32.add
        local.tee $local1
        local.get $lhs
        i32.add
      )
    )
    */

    const auto bin = fizzy::from_hex(
        "0061736d0100000001070160027f7f017f030201000a13011101017f200020016a20026a220220006a0b");
    const auto module = fizzy::parse(bin);

    const auto [trap, ret] = fizzy::execute(module, 0, {20, 22});

    ASSERT_EQ(trap, false);
    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 20 + 22 + 20);
}