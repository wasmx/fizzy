#include "parser.hpp"
#include <benchmark/benchmark.h>
#include <algorithm>
#include <random>
#include <vector>

std::pair<uint64_t, const uint8_t*> nop(const uint8_t* p, const uint8_t* end);
std::pair<uint64_t, const uint8_t*> leb128u_decode_u64_noinline(
    const uint8_t* input, const uint8_t* end);
std::pair<uint64_t, const uint8_t*> decodeULEB128(const uint8_t* p, const uint8_t* end);

fizzy::bytes leb128u_encode(uint64_t value);

namespace
{
template <typename T>
std::vector<T> generate_samples(size_t count)
{
    std::mt19937_64 gen{std::random_device{}()};
    std::uniform_int_distribution<T> dist;

    std::vector<T> samples;
    samples.reserve(count);
    std::generate_n(std::back_inserter(samples), count, [&] { return dist(gen); });
    return samples;
}
}  // namespace

template <decltype(fizzy::leb128u_decode<uint64_t>) Fn>
static void leb128u_decode_u64(benchmark::State& state)
{
    constexpr size_t size = 1024;
    const auto samples = generate_samples<uint64_t>(size);

    fizzy::bytes input;
    input.reserve(size * ((sizeof(uint64_t) * 8) / 7 + 1));
    for (const auto sample : samples)
        input += leb128u_encode(sample);

    benchmark::ClobberMemory();

    const auto end = &*std::cend(input);
    while (state.KeepRunningBatch(size))
    {
        auto pos = &*std::cbegin(input);
        for (size_t i = 0; i < size; ++i)
        {
            const auto r = Fn(pos, end);
            pos = r.second;
            benchmark::DoNotOptimize(r.first);
        }
        if (pos != end)
            state.SkipWithError("Not all input processed");
    }
}
BENCHMARK_TEMPLATE(leb128u_decode_u64, nop);
BENCHMARK_TEMPLATE(leb128u_decode_u64, fizzy::leb128u_decode<uint64_t>);
BENCHMARK_TEMPLATE(leb128u_decode_u64, leb128u_decode_u64_noinline);
BENCHMARK_TEMPLATE(leb128u_decode_u64, decodeULEB128);
