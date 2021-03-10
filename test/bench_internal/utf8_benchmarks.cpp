// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "utf8.hpp"
#include <benchmark/benchmark.h>
#include <test/utils/utf8_demo.hpp>

static void utf8_demo(benchmark::State& state)
{
    const auto utf8_demo_size = fizzy::test::utf8_demo.size();
    const auto utf8_demo_beg = reinterpret_cast<const uint8_t*>(fizzy::test::utf8_demo.data());
    const auto utf8_demo_end = utf8_demo_beg + utf8_demo_size;

    for ([[maybe_unused]] auto _ : state)
    {
        fizzy::utf8_validate(utf8_demo_beg, utf8_demo_end);
    }

    state.SetBytesProcessed(static_cast<int64_t>(utf8_demo_size));
}
BENCHMARK(utf8_demo);
