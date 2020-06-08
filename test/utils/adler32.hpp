// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "bytes.hpp"

namespace fizzy::test
{
// This calculates the Adler-32 checksum of the data.
// It is used in benchmarking.
inline uint32_t adler32(bytes_view data) noexcept
{
    constexpr uint32_t mod = 65521;
    uint32_t a = 1;
    uint32_t b = 0;

    for (const auto v : data)
    {
        a = (a + v) % mod;
        b = (b + a) % mod;
    }

    return (b << 16) | a;
}

}  // namespace fizzy::test
