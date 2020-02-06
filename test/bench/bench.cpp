#include <benchmark/benchmark.h>
#include <test/utils/hex.hpp>
#include <test/utils/wasm_engine.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <sstream>

namespace fs = std::filesystem;

namespace
{
constexpr auto WasmExtension = ".wasm";
constexpr auto InputsExtension = ".inputs";

using EngineCreateFn = decltype(&fizzy::test::create_fizzy_engine);

struct EngineRegistryEntry
{
    std::string_view name;
    EngineCreateFn create_fn;
};

constexpr EngineRegistryEntry engine_registry[] = {
    {"fizzy", fizzy::test::create_fizzy_engine},
};

inline std::string strip_space(const std::string& input)
{
    std::string result;
    std::copy_if(input.begin(), input.end(), std::back_inserter(result),
        [](auto x) noexcept { return !std::isspace(x); });
    return result;
}

void benchmark_parse(
    benchmark::State& state, EngineCreateFn create_fn, const fizzy::bytes& wasm_binary)
{
    const auto engine = create_fn();

    // Pre-run for validation
    if (!engine->parse(wasm_binary))
        return state.SkipWithError("Parsing failed");

    const auto input_size = wasm_binary.size();
    auto num_bytes_parsed = uint64_t{0};
    for (auto _ : state)  // NOLINT(clang-analyzer-deadcode.DeadStores)
    {
        engine->parse(wasm_binary);
        num_bytes_parsed += input_size;
    }
    state.counters["size"] = benchmark::Counter(static_cast<double>(input_size));
    state.counters["rate"] =
        benchmark::Counter(static_cast<double>(num_bytes_parsed), benchmark::Counter::kIsRate);
}

void benchmark_instantiate(
    benchmark::State& state, EngineCreateFn create_fn, const fizzy::bytes& wasm_binary)
{
    const auto engine = create_fn();

    if (!engine->parse(wasm_binary))
        return state.SkipWithError("Parsing failed");

    // Pre-run for validation
    if (!engine->instantiate())
        return state.SkipWithError("Instantiaton failed");

    for (auto _ : state)  // NOLINT(clang-analyzer-deadcode.DeadStores)
    {
        engine->instantiate();
    }
}

struct ExecutionBenchmarkCase
{
    std::shared_ptr<const fizzy::bytes> wasm_binary;
    std::string func_name;
    std::vector<uint64_t> func_args;
    fizzy::bytes memory;
    std::optional<uint64_t> expected_result;
    fizzy::bytes expected_memory;
};

void benchmark_execute(
    benchmark::State& state, EngineCreateFn create_fn, const ExecutionBenchmarkCase& benchmark_case)
{
    const auto engine = create_fn();
    if (!engine->parse(*benchmark_case.wasm_binary))
        return state.SkipWithError("Parsing failed");
    const auto func_ref = engine->find_function(benchmark_case.func_name);
    if (!func_ref)
    {
        return state.SkipWithError(
            ("Function \"" + benchmark_case.func_name + "\" not found").c_str());
    }

    if (!engine->instantiate())
        return state.SkipWithError("Instantiaton failed");

    auto initial_memory = fizzy::bytes{engine->get_memory()};

    // TODO: Check if memory is exported or imported.
    if (benchmark_case.memory.size() > initial_memory.size())
        return state.SkipWithError("Cannot init memory");

    std::copy(std::begin(benchmark_case.memory), std::end(benchmark_case.memory),
        std::begin(initial_memory));
    engine->set_memory(initial_memory);

    {  // Execute once and check results against expectations.
        const auto result = engine->execute(*func_ref, benchmark_case.func_args);
        if (result.trapped)
            return state.SkipWithError("Trapped");

        if (benchmark_case.expected_result)
        {
            if (!result.value)
                return state.SkipWithError("Missing result value");
            else if (*result.value != *benchmark_case.expected_result)
                return state.SkipWithError("Incorrect result");
        }
        else if (result.value)
            return state.SkipWithError("Unexpected result");

        const auto memory = engine->get_memory();
        if (memory.size() < benchmark_case.expected_memory.size())
            return state.SkipWithError("Result memory is shorter than expected");

        // Compare _beginning_ segment of the memory with expected.
        // Specifying expected full memory pages is impractical.
        if (!std::equal(std::begin(benchmark_case.expected_memory),
                std::end(benchmark_case.expected_memory), std::begin(memory)))
            return state.SkipWithError("Incorrect result memory");
    }

    for (auto _ : state)  // NOLINT(clang-analyzer-deadcode.DeadStores)
    {
        // Reset instance to its initial state.
        // At this point we only reset memory, so this works only while globals
        // and imports are not used. If this become a problem doing full
        // instantiate() should be considered.
        engine->set_memory(initial_memory);

        const auto result = engine->execute(*func_ref, benchmark_case.func_args);
        benchmark::DoNotOptimize(result);
    }
}

template <typename Lambda>
void register_benchmark(const std::string& name, Lambda&& fn)
{
#ifdef __clang_analyzer__
    // Clang analyzer reports potential memory leak in benchmark::RegisterBenchmark().
    // TODO: Upgrade benchmark library to newer version and recheck.
    (void)name;
    (void)fn;
#else
    benchmark::RegisterBenchmark(name.c_str(), std::forward<Lambda>(fn));
#endif
}

void load_benchmark(const fs::path& path, const std::string& name_prefix)
{
    const auto base_name = name_prefix + path.stem().string();

    std::ifstream wasm_file{path};
    const auto wasm_binary = std::make_shared<const fizzy::bytes>(
        std::istreambuf_iterator<char>{wasm_file}, std::istreambuf_iterator<char>{});

    for (const auto& entry : engine_registry)  // Register parse benchmarks.
    {
        register_benchmark(std::string{entry.name} + "/parse/" + base_name,
            [create_fn = entry.create_fn, wasm_binary](
                benchmark::State& state) { benchmark_parse(state, create_fn, *wasm_binary); });
    }

    for (const auto& entry : engine_registry)  // Register instantiate benchmark.
    {
        register_benchmark(std::string{entry.name} + "/instantiate/" + base_name,
            [create_fn = entry.create_fn, wasm_binary](benchmark::State& state) {
                benchmark_instantiate(state, create_fn, *wasm_binary);
            });
    }
    enum class InputsReadingState
    {
        Name,
        FuncName,
        FuncArguments,
        Memory,
        ExpectedResult,
        ExpectedMemory,
    };

    const auto inputs_path = fs::path{path}.replace_extension(InputsExtension);
    if (fs::exists(inputs_path))
    {
        auto st = InputsReadingState::Name;
        auto inputs_file = std::ifstream{inputs_path};
        std::string input_name;
        std::shared_ptr<ExecutionBenchmarkCase> benchmark_case;
        for (std::string l; std::getline(inputs_file, l);)
        {
            switch (st)
            {
            case InputsReadingState::Name:
                if (l.empty())
                    continue;
                input_name = std::move(l);
                benchmark_case = std::make_shared<ExecutionBenchmarkCase>();
                benchmark_case->wasm_binary = wasm_binary;
                st = InputsReadingState::FuncName;
                break;

            case InputsReadingState::FuncName:
                benchmark_case->func_name = std::move(l);
                st = InputsReadingState::FuncArguments;
                break;

            case InputsReadingState::FuncArguments:
            {
                std::istringstream iss{l};
                std::copy(std::istream_iterator<uint64_t>{iss}, std::istream_iterator<uint64_t>{},
                    std::back_inserter(benchmark_case->func_args));
                st = InputsReadingState::Memory;
                break;
            }

            case InputsReadingState::Memory:
                benchmark_case->memory = fizzy::from_hex(strip_space(l));
                st = InputsReadingState::ExpectedResult;
                break;

            case InputsReadingState::ExpectedResult:
                if (!l.empty())
                    benchmark_case->expected_result = std::stoull(l);
                st = InputsReadingState::ExpectedMemory;
                break;

            case InputsReadingState::ExpectedMemory:
                benchmark_case->expected_memory = fizzy::from_hex(strip_space(l));

                for (const auto& entry : engine_registry)
                {
                    register_benchmark(
                        std::string{entry.name} + "/execute/" + base_name + '/' + input_name,
                        [create_fn = entry.create_fn, benchmark_case](benchmark::State& state) {
                            benchmark_execute(state, create_fn, *benchmark_case);
                        });
                }

                benchmark_case = nullptr;
                st = InputsReadingState::Name;
                break;
            }
        }
    }
}

void load_benchmarks_from_dir(const fs::path& path, const std::string& name_prefix = {})
{
    std::vector<fs::path> subdirs;
    std::vector<fs::path> files;

    for (const auto& e : fs::directory_iterator{path})
    {
        if (e.is_directory())
            subdirs.emplace_back(e);
        else if (e.path().extension() == WasmExtension)
            files.emplace_back(e);
    }

    std::sort(std::begin(subdirs), std::end(subdirs));
    std::sort(std::begin(files), std::end(files));

    for (const auto& f : files)
        load_benchmark(f, name_prefix);

    for (const auto& d : subdirs)
        load_benchmarks_from_dir(d, name_prefix + d.filename().string() + '/');
}
}  // namespace

int main(int argc, char** argv)
{
    try
    {
        benchmark::Initialize(&argc, argv);

        if (argc < 2)
        {
            std::cerr << "Missing DIR argument\n";
            return -1;
        }
        else if (argc > 2)
        {
            benchmark::ReportUnrecognizedArguments(argc, argv);
            return -1;
        }

        load_benchmarks_from_dir(argv[1]);
        benchmark::RunSpecifiedBenchmarks();
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return -2;
    }
}
