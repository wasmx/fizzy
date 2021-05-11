// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "uvwasi.hpp"
#include "wasi.hpp"
#include <gtest/gtest.h>
#include <test/utils/hex.hpp>
#include <sstream>

using namespace fizzy;
using namespace fizzy::test;

TEST(wasi, destroy_non_inited_uvwasi)
{
    auto uvwasi = wasi::create_uvwasi();
}

TEST(wasi, init)
{
    const char* args[]{"ABC"};

    auto uvwasi = wasi::create_uvwasi();
    EXPECT_EQ(uvwasi->init(std::size(args), args), UVWASI_ESUCCESS);
}

TEST(wasi, init_multiple)
{
    auto uvwasi = wasi::create_uvwasi();

    const char* args1[]{"ABC"};
    EXPECT_EQ(uvwasi->init(std::size(args1), args1), UVWASI_ESUCCESS);

    const char* args2[]{"DEF"};
    EXPECT_EQ(uvwasi->init(std::size(args2), args2), UVWASI_ESUCCESS);
}

TEST(wasi, no_file)
{
    const char* args[]{"ABC"};

    std::ostringstream err;
    EXPECT_FALSE(wasi::load_and_run(std::size(args), args, err));
    EXPECT_EQ(err.str(), "File does not exist: \"ABC\"\n");
}
