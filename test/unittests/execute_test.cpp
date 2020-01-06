#include "execute.hpp"
#include <gtest/gtest.h>

TEST(execute, smoke)
{
    fizzy::module module;
    module.codesec.emplace_back(fizzy::code{1, {fizzy::instr::end}, {}});

    auto ret = fizzy::execute(module, 0, {});

    EXPECT_EQ(ret.size(), 0);
}
