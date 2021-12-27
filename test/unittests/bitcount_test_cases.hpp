// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <utility>

namespace fizzy::test
{
template <typename T>
struct BitCountTestCase
{
    T input;
    int popcount;
    int countl_zero;
    int countr_zero;
};

constexpr BitCountTestCase<uint32_t> bitcount32_test_cases[]{
    {0, 0, 32, 32},
    {1, 1, 31, 0},
    {0x7f, 7, 25, 0},
    {0x80, 1, 24, 7},
    {0x12345678, 13, 3, 3},
    {0xffffffff, 32, 0, 0},
    {0xffff0000, 16, 0, 16},
    {0x0000ffff, 16, 16, 0},
    {0x00ffff00, 16, 8, 8},
    {0x00ff00ff, 16, 8, 0},
    {0x007f8001, 9, 9, 0},
    {0x0055ffaa, 16, 9, 1},
};

constexpr BitCountTestCase<uint64_t> bitcount64_test_cases[]{
    {0, 0, 64, 64},
    {1, 1, 63, 0},
    {0x7f, 7, 57, 0},
    {0x80, 1, 56, 7},
    {0x1234567890abcdef, 32, 3, 0},
    {0xffffffffffffffff, 64, 0, 0},
    {0xffffffff00000000, 32, 0, 32},
    {0x00000000ffffffff, 32, 32, 0},
    {0x0000ffffffff0000, 32, 16, 16},
    {0x00ff00ff00ff00ff, 32, 8, 0},
    {0x007f8001007f8001, 18, 9, 0},
    {0x0055ffaa0055ffaa, 32, 9, 1},
};

}  // namespace fizzy::test
