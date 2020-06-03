// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "span.hpp"
#include <benchmark/benchmark.h>
#include <memory>
#include <vector>

namespace
{
[[gnu::noinline]] auto init_locals_1(fizzy::span<const uint64_t> args, uint32_t local_count)
{
    std::vector<uint64_t> locals;
    locals.reserve(args.size() + local_count);
    std::copy_n(args.begin(), args.size(), std::back_inserter(locals));
    locals.resize(locals.size() + local_count);
    return locals;
}

[[gnu::noinline]] auto init_locals_2(fizzy::span<const uint64_t> args, uint32_t local_count)
{
    std::vector<uint64_t> locals(args.size() + local_count);
    std::copy_n(args.begin(), args.size(), locals.begin());
    return locals;
}

[[gnu::noinline]] auto init_locals_3(fizzy::span<const uint64_t> args, uint32_t local_count)
{
    std::vector<uint64_t> locals(args.size() + local_count);
    __builtin_memcpy(locals.data(), args.data(), args.size());
    return locals;
}

[[gnu::noinline]] auto init_locals_4(fizzy::span<const uint64_t> args, uint32_t local_count)
{
    auto locals = std::make_unique<uint64_t[]>(args.size() + local_count);
    std::copy_n(args.begin(), args.size(), &locals[0]);
    std::fill_n(&locals[args.size()], local_count, 0);
    return locals;
}

[[gnu::noinline]] auto init_locals_5(fizzy::span<const uint64_t> args, uint32_t local_count)
{
    auto locals = std::make_unique<uint64_t[]>(args.size() + local_count);
    __builtin_memcpy(locals.get(), args.data(), args.size());
    __builtin_memset(locals.get() + args.size(), 0, local_count);
    return locals;
}
}  // namespace

template <typename T, T Fn(fizzy::span<const uint64_t>, uint32_t)>
static void init_locals(benchmark::State& state)
{
    const auto num_args = static_cast<size_t>(state.range(0));
    const auto num_locals = static_cast<uint32_t>(state.range(1));

    const std::vector<uint64_t> args(num_args, 0xa49);
    benchmark::ClobberMemory();

    for ([[maybe_unused]] auto _ : state)
    {
        const auto locals = Fn(args, num_locals);
        benchmark::DoNotOptimize(locals);
    }
}
#define ARGS            \
    Args({0, 0})        \
        ->Args({2, 4})  \
        ->Args({2, 38}) \
        ->Args({3, 4})  \
        ->Args({3, 8})  \
        ->Args({3, 13}) \
        ->Args({5, 30}) \
        ->Args({10, 100})
BENCHMARK_TEMPLATE(init_locals, std::vector<uint64_t>, init_locals_1)->ARGS;
BENCHMARK_TEMPLATE(init_locals, std::vector<uint64_t>, init_locals_2)->ARGS;
BENCHMARK_TEMPLATE(init_locals, std::vector<uint64_t>, init_locals_3)->ARGS;
BENCHMARK_TEMPLATE(init_locals, std::unique_ptr<uint64_t[]>, init_locals_4)->ARGS;
BENCHMARK_TEMPLATE(init_locals, std::unique_ptr<uint64_t[]>, init_locals_5)->ARGS;
