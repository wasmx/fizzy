#include "execute.hpp"
#include "parser.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
constexpr auto JsonExtension = ".json";

template <typename T>
uint64_t json_to_value(const json& v)
{
    return static_cast<uint64_t>(static_cast<T>(std::stoull(v.get<std::string>())));
}

class test_runner
{
public:
    void run_from_file(const fs::path& path)
    {
        std::cout << "Running tests from " << path << "\n";

        std::ifstream test_file{path};
        const json j = json::parse(test_file);

        for (const auto& cmd : j.at("commands"))
        {
            const auto type = cmd.at("type").get<std::string>();

            std::cout << "Line " << cmd.at("line").get<int>() << ": " << type << " " << std::flush;

            if (type == "module")
            {
                const auto filename = cmd.at("filename").get<std::string>();
                std::cout << "Instantiating " << filename << "\n";

                std::ifstream wasm_file{fs::path{path}.replace_filename(filename)};

                const auto wasm_binary = fizzy::bytes(
                    std::istreambuf_iterator<char>{wasm_file}, std::istreambuf_iterator<char>{});

                try
                {
                    instance = fizzy::instantiate(fizzy::parse(wasm_binary));
                }
                catch (const fizzy::parser_error& ex)
                {
                    std::cout << "FAILED Parsing failed with error: " << ex.what() << "\n";
                    continue;
                }
                catch (const fizzy::instantiate_error& ex)
                {
                    std::cout << "FAILED Instantiation failed with error: " << ex.what() << "\n";
                    continue;
                }
            }
            else if (type == "assert_return")
            {
                const auto& action = cmd.at("action");
                const auto action_type = action.at("type").get<std::string>();
                if (action_type == "invoke")
                {
                    auto result = invoke(action);
                    if (!result.has_value())
                        continue;

                    if (result->trapped)
                    {
                        std::cout << "FAILED Function trapped.\n";
                        continue;
                    }

                    const auto& expected = cmd.at("expected");
                    if (expected.empty() && !result->stack.empty())
                    {
                        std::cout << "FAILED Unexpected returned value.\n";
                        continue;
                    }

                    if (result->stack.size() != 1)
                    {
                        std::cout << "FAILED More than 1 value returned.\n";
                        continue;
                    }

                    const auto expected_type = expected.at(0).at("type").get<std::string>();
                    uint64_t expected_value;
                    uint64_t actual_value;
                    if (expected_type == "i32")
                    {
                        expected_value = json_to_value<int32_t>(expected.at(0).at("value"));
                        actual_value =
                            static_cast<uint64_t>(static_cast<int32_t>(result->stack[0]));
                    }
                    else if (expected_type == "i64")
                    {
                        expected_value = json_to_value<int64_t>(expected.at(0).at("value"));
                        actual_value = result->stack[0];
                    }
                    else
                    {
                        std::cout << "SKIPPED Unsupported expected type '" << expected_type
                                  << "'.\n";
                        continue;
                    }

                    if (expected_value != actual_value)
                    {
                        std::cout << "FAILED Incorrect returned value. Expected: " << expected_value
                                  << " (0x" << std::hex << expected_value
                                  << ") Actual: " << std::dec << actual_value << " (0x" << std::hex
                                  << actual_value << std::dec << ")\n";
                        continue;
                    }

                    std::cout << "PASSED\n";
                }
                else
                    std::cout << "SKIPPED Unsupported action type '" << action_type << "'\n";
            }
            else if (type == "assert_trap")
            {
                const auto& action = cmd.at("action");
                const auto action_type = action.at("type").get<std::string>();
                if (action_type != "invoke")
                    std::cout << "SKIPPED Unsupported action type '" << action_type << "'\n";

                auto result = invoke(action);
                if (!result.has_value())
                    continue;

                if (!result->trapped)
                {
                    std::cout << "FAILED Function expected to trap, but it didn't.\n";
                    continue;
                }

                std::cout << "PASSED\n";
            }
            else
                std::cout << "SKIPPED Unsupported command type\n";
        }
    }

private:
    std::optional<fizzy::execution_result> invoke(const json& action)
    {
        const auto func_name = action.at("field").get<std::string>();
        const auto func_idx = fizzy::find_exported_function(instance.module, func_name);
        if (!func_idx.has_value())
        {
            std::cout << "SKIPPED Function '" << func_name << "' not found.\n";
            return std::nullopt;
        }

        std::vector<uint64_t> args;
        for (const auto& arg : action.at("args"))
        {
            const auto arg_type = arg.at("type").get<std::string>();
            uint64_t arg_value;
            if (arg_type == "i32")
                arg_value = json_to_value<int32_t>(arg.at("value"));
            else if (arg_type == "i64")
                arg_value = json_to_value<int64_t>(arg.at("value"));
            else
            {
                std::cout << "SKIPPED Unsupported argument type '" << arg_type << "'.\n";
                return std::nullopt;
            }
            args.push_back(arg_value);
        }

        return fizzy::execute(instance, *func_idx, std::move(args));
    }

    fizzy::Instance instance;
};

void run_tests_from_dir(const fs::path& path)
{
    std::vector<fs::path> files;
    for (const auto& e : fs::recursive_directory_iterator{path})
    {
        if (e.is_regular_file() && e.path().extension() == JsonExtension)
            files.emplace_back(e);
    }

    std::sort(std::begin(files), std::end(files));

    for (const auto& f : files)
    {
        try
        {
            test_runner{}.run_from_file(f);
        }
        catch (const std::exception& ex)
        {
            std::cerr << "Exception: " << ex.what() << "\n\n";
        }
    }
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Missing DIR argument\n";
            return -1;
        }
        else if (argc > 2)
        {
            std::cerr << "Too many arguments\n";
            return -1;
        }

        run_tests_from_dir(argv[1]);
        return 0;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return -2;
    }
}
