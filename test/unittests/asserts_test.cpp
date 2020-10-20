// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "asserts.hpp"
#include <gtest/gtest.h>

#if !defined(NDEBUG) || __has_feature(undefined_behavior_sanitizer)
TEST(asserts_death, unreachable)
{
    static constexpr auto unreachable = []() noexcept { FIZZY_UNREACHABLE(); };
    EXPECT_DEATH(unreachable(), "unreachable");
}
#endif
