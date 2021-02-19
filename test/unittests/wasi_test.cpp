// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "wasi.hpp"
#include <gtest/gtest.h>
#include <test/utils/hex.hpp>
#include <sstream>

using namespace fizzy;
using namespace fizzy::test;

TEST(wasi, no_file)
{
    const char* args[]{"ABC"};

    std::ostringstream err;
    EXPECT_FALSE(wasi::load_and_run(std::size(args), args, err));
    EXPECT_EQ(err.str(), "File does not exist: \"ABC\"\n");
}
