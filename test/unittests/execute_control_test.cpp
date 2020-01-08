#include "execute.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

TEST(execute_control, unreachable)
{
    Module module;
    module.codesec.emplace_back(Code{0, {Instr::unreachable, Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_TRUE(trap);
}

TEST(execute_control, nop)
{
    Module module;
    module.codesec.emplace_back(Code{0, {Instr::nop, Instr::end}, {}});

    const auto [trap, ret] = execute(module, 0, {});

    ASSERT_FALSE(trap);
    EXPECT_EQ(ret.size(), 0);
}