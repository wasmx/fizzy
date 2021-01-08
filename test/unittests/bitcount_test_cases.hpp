// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <utility>

namespace fizzy::test
{
constexpr std::pair<uint32_t, uint32_t> popcount32_test_cases[]{
    {0, 0},
    {1, 1},
    {0x7f, 7},
    {0x80, 1},
    {0x12345678, 13},
    {0xffffffff, 32},
    {0xffff0000, 16},
    {0x0000ffff, 16},
    {0x00ffff00, 16},
    {0x00ff00ff, 16},
    {0x007f8001, 9},
    {0x0055ffaa, 16},
};

constexpr std::pair<uint64_t, uint64_t> popcount64_test_cases[]{
    {0, 0},
    {1, 1},
    {0x7f, 7},
    {0x80, 1},
    {0x1234567890abcdef, 32},
    {0xffffffffffffffff, 64},
    {0xffffffff00000000, 32},
    {0x00000000ffffffff, 32},
    {0x0000ffffffff0000, 32},
    {0x00ff00ff00ff00ff, 32},
    {0x007f8001007f8001, 18},
    {0x0055ffaa0055ffaa, 32},
};
}  // namespace fizzy::test
