#include "parser.hpp"
#include <benchmark/benchmark.h>
#include <test/utils/leb128_encode.hpp>
#include <algorithm>
#include <random>
#include <vector>

std::pair<uint64_t, const uint8_t*> nop(const uint8_t* p, const uint8_t* end);
std::pair<uint64_t, const uint8_t*> leb128u_decode_u64_noinline(
    const uint8_t* input, const uint8_t* end);
std::pair<uint64_t, const uint8_t*> decodeULEB128(const uint8_t* p, const uint8_t* end);

namespace
{
std::mt19937_64 g_gen{std::random_device{}()};

template <typename T>
std::vector<T> generate_samples(size_t count)
{
    std::uniform_int_distribution<T> dist;

    std::vector<T> samples;
    samples.reserve(count);
    std::generate_n(std::back_inserter(samples), count, [&] { return dist(g_gen); });
    return samples;
}

fizzy::bytes generate_ascii_vec(size_t size)
{
    std::uniform_int_distribution<uint8_t> dist{0, 0x7f};

    const auto size_encoded = fizzy::test::leb128u_encode(size);
    fizzy::bytes result;
    result.reserve(size + size_encoded.size());
    result += size_encoded;
    std::generate_n(std::back_inserter(result), size, [&] { return dist(g_gen); });
    return result;
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
        input += fizzy::test::leb128u_encode(sample);

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

static void parse_string(benchmark::State& state)
{
    const auto size = static_cast<size_t>(state.range(0));
    const auto input = generate_ascii_vec(size);
    const auto input_begin = input.data();
    const auto input_end = input_begin + input.size();
    benchmark::ClobberMemory();

    for ([[maybe_unused]] auto _ : state)
    {
        fizzy::parse_string(input_begin, input_end);
    }

    state.SetItemsProcessed(static_cast<int64_t>(size));
}
BENCHMARK(parse_string)->RangeMultiplier(2)->Range(16, 4 * 1024);
