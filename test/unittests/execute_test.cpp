#include "execute.hpp"
#include <gtest/gtest.h>

TEST(execute, smoke)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{1, {fizzy::instr::end}, {}});

    auto ret = fizzy::execute(module, 0, {});

    EXPECT_EQ(ret.size(), 0);
}

TEST(execute, basic_add)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{1,
        {fizzy::instr::local_get, fizzy::instr::local_get, fizzy::instr::i32_add,
            fizzy::instr::end},
        {0, 0, 0, 0, 0, 0, 0, 0}});

    auto ret = fizzy::execute(module, 0, {});

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

    auto ret = fizzy::execute(module, 0, {20, 22});

    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_set)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0,
        {fizzy::instr::local_get, fizzy::instr::local_get, fizzy::instr::i32_add,
            fizzy::instr::local_set, fizzy::instr::local_get, fizzy::instr::end},
        {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}});

    auto ret = fizzy::execute(module, 0, {20, 22});

    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}

TEST(execute, local_tee)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{0,
        {fizzy::instr::local_get, fizzy::instr::local_get, fizzy::instr::i32_add,
            fizzy::instr::local_tee, fizzy::instr::end},
        {0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0}});

    auto ret = fizzy::execute(module, 0, {20, 22});

    ASSERT_EQ(ret.size(), 1);
    EXPECT_EQ(ret[0], 42);
}
