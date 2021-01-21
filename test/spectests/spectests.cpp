// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include <nlohmann/json.hpp>
#include <test/utils/floating_point_utils.hpp>
#include <test/utils/hex.hpp>
#include <test/utils/typed_value.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <type_traits>
#include <unordered_map>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace
{
constexpr auto JsonExtension = ".json";
constexpr auto UnnamedModule = "_unnamed";
constexpr unsigned TestMemoryPagesLimit = (4 * 1024 * 1024 * 1024ULL) / fizzy::PageSize;  // 4 Gb

// spectest module definition:
// https://github.com/WebAssembly/spec/blob/99564b7eaa3452c2633b623c92fc286db2823f39/interpreter/README.md#spectest-host-module
/* wat2wasm
(module
  (func (export "print"))
  (func (export "print_i32") (param i32))
  (func (export "print_i64") (param i64))
  (func (export "print_i32_f32") (param i32) (param f32))
  (func (export "print_f64_f64") (param f64) (param f64))
  (func (export "print_f32") (param f32))
  (func (export "print_f64") (param f64))
  (global (export "global_i32") i32 (i32.const 666))
  (global (export "global_i64") i64 (i64.const 666))
  (global (export "global_f32") f32 (f32.const 666))
  (global (export "global_f64") f64 (f64.const 666))
  (table (export "table") 10 20 anyfunc)
  (memory (export "memory") 1 2)
)
*/
const auto spectest_bin = fizzy::test::from_hex(
    "0061736d01000000011e0760000060017f0060017e0060027f7d0060027c7c0060017d0060017c0003080700010203"
    "04050604050170010a140504010101020621047f00419a050b7e00429a050b7d0043008026440b7c00440000000000"
    "d084400b079e010d057072696e740000097072696e745f6933320001097072696e745f69363400020d7072696e745f"
    "6933325f66333200030d7072696e745f6636345f6636340004097072696e745f6633320005097072696e745f663634"
    "00060a676c6f62616c5f69333203000a676c6f62616c5f69363403010a676c6f62616c5f66333203020a676c6f6261"
    "6c5f6636340303057461626c650100066d656d6f727902000a160702000b02000b02000b02000b02000b02000b0200"
    "0b");
const auto spectest_module = fizzy::parse(spectest_bin);
const std::string spectest_name = "spectest";

fizzy::bytes load_wasm_file(const fs::path& json_file_path, std::string_view filename)
{
    std::ifstream wasm_file{fs::path{json_file_path}.replace_filename(filename)};

    return fizzy::bytes(
        std::istreambuf_iterator<char>{wasm_file}, std::istreambuf_iterator<char>{});
}

struct test_settings
{
    bool skip_validation = false;
    bool show_passed = false;
    bool show_failed = true;
    bool show_skipped = false;
};

struct test_results
{
    int passed = 0;
    int failed = 0;
    int skipped = 0;
};

struct imports
{
    std::vector<fizzy::ExternalFunction> functions;
    std::vector<fizzy::ExternalTable> tables;
    std::vector<fizzy::ExternalMemory> memories;
    std::vector<fizzy::ExternalGlobal> globals;
};

class test_runner
{
public:
    explicit test_runner(const test_settings& ts)
      : m_settings{ts}, m_registered_names{{spectest_name, spectest_name}}
    {
        m_instances[spectest_name] =
            fizzy::instantiate(std::make_unique<fizzy::Module>(*spectest_module));
    }

    test_results run_from_file(const fs::path& path)
    {
        log("Running tests from " + path.string());

        std::ifstream test_file{path};
        const json j = json::parse(test_file);

        for (const auto& cmd : j.at("commands"))
        {
            const auto type = cmd.at("type").get<std::string>();

            m_current_line = cmd.at("line").get<int>();
            m_current_test_type = type;

            if (type == "module")
            {
                const auto filename = cmd.at("filename").get<std::string>();

                const std::string name =
                    (cmd.find("name") != cmd.end() ? cmd.at("name").get<std::string>() :
                                                     UnnamedModule);

                const auto wasm_binary = load_wasm_file(path, filename);
                try
                {
                    auto module = fizzy::parse(wasm_binary);

                    auto [imports, error] = create_imports(*module);
                    if (!error.empty())
                    {
                        fail(error);
                        m_instances.erase(name);
                        continue;
                    }

                    m_instances[name] =
                        fizzy::instantiate(std::move(module), std::move(imports.functions),
                            std::move(imports.tables), std::move(imports.memories),
                            std::move(imports.globals), TestMemoryPagesLimit);

                    m_last_module_name = name;
                }
                catch (const fizzy::parser_error& ex)
                {
                    fail(std::string{"Parsing failed with error: "} + ex.what());
                    m_instances.erase(name);
                    m_last_module_name.clear();
                    continue;
                }
                catch (const fizzy::validation_error& ex)
                {
                    fail(std::string{"Validation failed with error: "} + ex.what());
                    m_instances.erase(name);
                    m_last_module_name.clear();
                    continue;
                }
                catch (const fizzy::instantiate_error& ex)
                {
                    fail(std::string{"Instantiation failed with error: "} + ex.what());
                    m_instances.erase(name);
                    m_last_module_name.clear();
                    continue;
                }
                pass();
            }
            else if (type == "register")
            {
                const auto module_name =
                    (cmd.find("name") != cmd.end() ? cmd["name"] : UnnamedModule);

                const auto it_instance = m_instances.find(module_name);
                if (it_instance == m_instances.end())
                {
                    skip("Module not found.");
                    continue;
                }

                const auto registered_name = cmd.at("as").get<std::string>();

                if (module_name == UnnamedModule)
                {
                    // Assign a name to unnamed module to avoid it being overwritten by another
                    // unnamed one. Let the name be equal to registered name.
                    const auto [_, inserted] =
                        m_instances.emplace(registered_name, std::move(it_instance->second));
                    if (!inserted)
                    {
                        fail("Failed to register unnamed module - another module with name \"" +
                             registered_name + "\" already exists");
                        continue;
                    }

                    m_instances.erase(module_name);
                    m_registered_names[registered_name] = registered_name;
                }
                else
                    m_registered_names[registered_name] = module_name;
                pass();
            }
            else if (type == "assert_return" || type == "action")
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
                        fail("Function trapped.");
                        continue;
                    }

                    const auto& expected = cmd.at("expected");
                    if (expected.empty())
                    {
                        if (!result->has_value)
                            pass();
                        else
                            fail("Unexpected returned value.");
                        continue;
                    }

                    if (!check_result(result->value, expected.at(0)))
                        continue;

                    pass();
                }
                else if (action_type == "get")
                {
                    auto instance = find_instance_for_action(action);
                    if (!instance)
                        continue;

                    const auto global_name = action.at("field").get<std::string>();
                    const auto global = fizzy::find_exported_global(*instance, global_name);
                    if (!global)
                    {
                        fail("Global \"" + global_name + "\" not found.");
                        continue;
                    }

                    if (!check_result(*global->value, cmd.at("expected").at(0)))
                        continue;

                    pass();
                }
                else
                    skip("Unsupported action type '" + action_type + "'");
            }
            else if (type == "assert_trap" || type == "assert_exhaustion")
            {
                const auto& action = cmd.at("action");
                const auto action_type = action.at("type").get<std::string>();
                if (action_type != "invoke")
                {
                    skip("Unsupported action type '" + action_type + "'");
                    continue;
                }

                auto result = invoke(action);
                if (!result.has_value())
                    continue;

                if (!result->trapped)
                {
                    fail("Function expected to trap, but it didn't.");
                    continue;
                }

                pass();
            }
            else if (type == "assert_invalid" || type == "assert_malformed")
            {
                // NOTE: assert_malformed should result in a parser error and
                //       assert_invalid should result in a validation error
                if (type == "assert_invalid" && m_settings.skip_validation)
                {
                    skip("Validation tests disabled.");
                    continue;
                }

                const auto module_type = cmd.at("module_type").get<std::string>();
                if (module_type != "binary")
                {
                    skip("Only binary modules are supported.");
                    continue;
                }

                const auto filename = cmd.at("filename").get<std::string>();
                const auto wasm_binary = load_wasm_file(path, filename);
                try
                {
                    fizzy::parse(wasm_binary);
                }
                catch (fizzy::parser_error const& ex)
                {
                    if (type == "assert_malformed")
                        pass(ex.what());
                    else
                        fail(std::string{"Unexpected parser error: "} + ex.what());
                    continue;
                }
                catch (fizzy::validation_error const& ex)
                {
                    if (type == "assert_invalid")
                        pass(ex.what());
                    else
                        fail(std::string{"Unexpected validation error: "} + ex.what());
                    continue;
                }

                fail("Invalid module parsed successfully. Expected error: " +
                     cmd.at("text").get<std::string>());
            }
            else if (type == "assert_unlinkable" || type == "assert_uninstantiable")
            {
                // NOTE: assert_uninstantiable should result in a start function trap
                //       assert_unlinkable checks all other instantiation failures

                const auto module_type = cmd.at("module_type").get<std::string>();
                if (module_type != "binary")
                {
                    skip("Only binary modules are supported.");
                    continue;
                }

                const auto filename = cmd.at("filename").get<std::string>();
                const auto wasm_binary = load_wasm_file(path, filename);
                try
                {
                    auto module = fizzy::parse(wasm_binary);

                    auto [imports, error] = create_imports(*module);
                    if (!error.empty())
                    {
                        if (type == "assert_unlinkable")
                            pass(error);
                        else
                            fail(error);
                        continue;
                    }

                    fizzy::instantiate(std::move(module), std::move(imports.functions),
                        std::move(imports.tables), std::move(imports.memories),
                        std::move(imports.globals));
                }
                catch (const fizzy::parser_error& ex)
                {
                    fail(std::string{"Parsing failed with error: "} + ex.what());
                    continue;
                }
                catch (const fizzy::validation_error& ex)
                {
                    fail(std::string{"Validation failed with error: "} + ex.what());
                    continue;
                }
                catch (const fizzy::instantiate_error& ex)
                {
                    if (ex.what() == std::string{"start function failed to execute"})
                    {
                        if (type == "assert_uninstantiable")
                            pass(ex.what());
                        else
                            fail(std::string{"Instantiation failed with error: "} + ex.what());
                    }
                    else
                    {
                        if (type == "assert_uninstantiable")
                            fail(std::string{"Instantiation failed with error: "} + ex.what());
                        else
                            pass(ex.what());
                    }
                    continue;
                }

                fail("Module instantiated successfully. Expected error: " +
                     cmd.at("text").get<std::string>());
            }
            else
                skip("Unsupported command type");
        }

        log("");  // newline after dots line
        if (!m_result_details.empty())
            log_no_newline(m_result_details);
        log(std::to_string(m_results.passed + m_results.failed + m_results.skipped) +
            " tests ran from " + path.filename().string() + ".\n  PASSED " +
            std::to_string(m_results.passed) + ", FAILED " + std::to_string(m_results.failed) +
            ", SKIPPED " + std::to_string(m_results.skipped) + ".\n");

        return m_results;
    }

private:
    fizzy::Instance* find_instance_for_action(const json& action)
    {
        const auto module_name =
            (action.find("module") != action.end() ? action["module"].get<std::string>() :
                                                     m_last_module_name);

        const auto it_instance = m_instances.find(module_name);
        if (it_instance == m_instances.end())
        {
            skip("No instantiated module.");
            return nullptr;
        }

        return it_instance->second.get();
    }

    std::optional<fizzy::test::TypedValue> read_value(const json& v)
    {
        const auto type = v.at("type").get<std::string>();

        // JSON tests have all values including floats serialized as 64-bit unsigned integers.
        const uint64_t raw_value = std::stoull(v.at("value").get<std::string>());

        if (type == "i32")
        {
            assert(static_cast<uint32_t>(raw_value) == raw_value && "overflow in i32 test value");
            return static_cast<uint32_t>(raw_value);
        }
        else if (type == "i64")
        {
            return raw_value;
        }
        else if (type == "f32")
        {
            assert(static_cast<uint32_t>(raw_value) == raw_value && "overflow in f32 test value");
            assert(!is_canonical_nan(v) && !is_arithmetic_nan(v));
            return fizzy::test::FP{static_cast<uint32_t>(raw_value)}.value;
        }
        else if (type == "f64")
        {
            assert(!is_canonical_nan(v) && !is_arithmetic_nan(v));
            return fizzy::test::FP{raw_value}.value;
        }
        else
        {
            skip("Unsupported value type '" + type + "'.");
            return std::nullopt;
        }
    }

    static bool is_canonical_nan(const json& v)
    {
        return v.at("value").get<std::string>() == "nan:canonical";
    }

    static bool is_arithmetic_nan(const json& v)
    {
        return v.at("value").get<std::string>() == "nan:arithmetic";
    }

    std::optional<fizzy::ExecutionResult> invoke(const json& action)
    {
        auto instance = find_instance_for_action(action);
        if (!instance)
            return std::nullopt;

        const auto func_name = action.at("field").get<std::string>();
        const auto func_idx = fizzy::find_exported_function(*instance->module, func_name);
        if (!func_idx.has_value())
        {
            skip("Function '" + func_name + "' not found.");
            return std::nullopt;
        }

        std::vector<fizzy::Value> args;
        for (const auto& arg : action.at("args"))
        {
            const auto arg_value = read_value(arg);
            if (!arg_value.has_value())
                return std::nullopt;

            args.push_back(arg_value->value);
        }

        assert(args.size() == instance->module->get_function_type(*func_idx).inputs.size());
        // TODO: Switch to fizzy::test::execute() to check argument types.
        return fizzy::execute(*instance, *func_idx, args.data());
    }

    template <typename T>
    bool check_integer_result(fizzy::Value value, const json& expected)
    {
        const auto expected_value = read_value(expected)->value.as<T>();
        const auto actual_value = value.as<T>();

        if (expected_value != actual_value)
        {
            std::stringstream message;
            message << "Incorrect returned value. Expected: " << expected_value << " (0x"
                    << std::hex << expected_value << ")"
                    << " Actual: " << std::dec << actual_value << " (0x" << std::hex << actual_value
                    << ")";
            fail(message.str());
            return false;
        }
        return true;
    }

    template <typename T>
    bool check_floating_point_result(fizzy::Value actual_value, const json& expected)
    {
        const auto fp_actual = fizzy::test::FP{actual_value.as<T>()};

        const bool is_canonical_nan_expected = is_canonical_nan(expected);
        if (is_canonical_nan_expected && fp_actual.is_canonical_nan())
            return true;
        const bool is_arithmetic_nan_expected = is_arithmetic_nan(expected);
        if (is_arithmetic_nan_expected && fp_actual.is_arithmetic_nan())
            return true;

        if (is_canonical_nan_expected || is_arithmetic_nan_expected)
        {
            std::stringstream message;
            message << "Incorrect returned value. Expected: "
                    << expected.at("value").get<std::string>()
                    << " Actual: " << actual_value.as<T>() << " (" << std::hexfloat
                    << actual_value.as<T>() << ")";
            fail(message.str());
            return false;
        }

        const auto expected_value = read_value(expected)->value;  // TODO: Type ignored.

        if (expected_value.template as<T>() != fp_actual)
        {
            std::stringstream message;
            message << "Incorrect returned value. Expected: " << expected_value.template as<T>()
                    << " (" << std::hexfloat << expected_value.template as<T>() << ")"
                    << " Actual: " << std::defaultfloat << actual_value.as<T>() << " ("
                    << std::hexfloat << actual_value.as<T>() << ")";
            fail(message.str());
            return false;
        }
        return true;
    }

    bool check_result(fizzy::Value actual_value, const json& expected)
    {
        // TODO: Check types here.
        // TODO: Use read_value() here too if it can handle "nan:..." strings.
        const auto type = expected.at("type").get<std::string>();

        if (type == "i32")
            return check_integer_result<uint32_t>(actual_value, expected);
        else if (type == "i64")
            return check_integer_result<uint64_t>(actual_value, expected);
        else if (type == "f32")
            return check_floating_point_result<float>(actual_value, expected);
        else if (type == "f64")
            return check_floating_point_result<double>(actual_value, expected);
        else
        {
            skip("Unsupported value type '" + type + "'.");
            return false;
        }
    }

    std::pair<imports, std::string> create_imports(const fizzy::Module& module)
    {
        imports result;
        for (const auto& import : module.importsec)
        {
            const auto it_registered = m_registered_names.find(import.module);
            if (it_registered == m_registered_names.end())
                return {{}, "Module \"" + import.module + "\" not registered."};

            const auto module_name = it_registered->second;
            const auto it_instance = m_instances.find(module_name);
            if (it_instance == m_instances.end())
                return {{}, "Module not instantiated."};

            auto& instance = it_instance->second;

            if (import.kind == fizzy::ExternalKind::Function)
            {
                const auto func = fizzy::find_exported_function(*instance, import.name);
                if (!func.has_value())
                {
                    return {{},
                        "Function \"" + import.name + "\" not found in \"" + import.module + "\"."};
                }

                result.functions.emplace_back(*func);
            }
            else if (import.kind == fizzy::ExternalKind::Table)
            {
                const auto table = fizzy::find_exported_table(*instance, import.name);
                if (!table.has_value())
                {
                    return {{},
                        "Table \"" + import.name + "\" not found in \"" + import.module + "\"."};
                }

                result.tables.emplace_back(*table);
            }
            else if (import.kind == fizzy::ExternalKind::Memory)
            {
                const auto memory = fizzy::find_exported_memory(*instance, import.name);
                if (!memory.has_value())
                {
                    return {{},
                        "Memory \"" + import.name + "\" not found in \"" + import.module + "\"."};
                }

                result.memories.emplace_back(*memory);
            }
            else if (import.kind == fizzy::ExternalKind::Global)
            {
                const auto global = fizzy::find_exported_global(*instance, import.name);
                if (!global.has_value())
                {
                    return {{},
                        "Global \"" + import.name + "\" not found in \"" + import.module + "\"."};
                }

                result.globals.emplace_back(*global);
            }
        }

        return {result, ""};
    }

    void pass(std::string_view message = {})
    {
        ++m_results.passed;
        if (m_settings.show_passed)
            add_to_result_details("PASSED " + std::string{message});
        log_no_newline(".");
    }

    void fail(std::string_view message)
    {
        ++m_results.failed;
        if (m_settings.show_failed)
            add_to_result_details("FAILED " + std::string{message});
        log_no_newline("F");
    }

    void skip(std::string_view message)
    {
        ++m_results.skipped;
        if (m_settings.show_skipped)
            add_to_result_details("SKIPPED " + std::string{message});
        log_no_newline("s");
    }

    static void log(std::string_view message) { std::cout << message << "\n"; }

    static void log_no_newline(std::string_view message) { std::cout << message << std::flush; }

    void add_to_result_details(std::string_view message)
    {
        assert(!m_current_test_type.empty() && m_current_line != 0);
        m_result_details += "Line " + std::to_string(m_current_line) + ": " + m_current_test_type +
                            " " + std::string(message) + "\n";
        m_current_line = 0;
        m_current_test_type.clear();
    }

    test_settings m_settings;
    std::unordered_map<std::string, std::unique_ptr<fizzy::Instance>> m_instances;
    std::unordered_map<std::string, std::string> m_registered_names;
    std::string m_last_module_name;
    test_results m_results;
    std::string m_result_details;
    int m_current_line = 0;
    std::string m_current_test_type;
};

void log_total(const fs::path& path, const test_results& res)
{
    std::cout << "TOTAL " << (res.passed + res.failed + res.skipped) << " tests ran from " << path
              << ".\n  PASSED " << res.passed << ", FAILED " << res.failed << ", SKIPPED "
              << res.skipped << ".\n";
}

bool run_tests_from_file(const fs::path& path, const test_settings& settings)
{
    const auto res = test_runner{settings}.run_from_file(path);

    log_total(path, res);
    return res.failed == 0;
}

bool run_tests_from_dir(const fs::path& path, const test_settings& settings)
{
    std::vector<fs::path> files;
    for (const auto& e : fs::recursive_directory_iterator{path})
    {
        if (e.is_regular_file() && e.path().extension() == JsonExtension)
            files.emplace_back(e);
    }

    std::sort(std::begin(files), std::end(files));

    test_results total;
    for (const auto& f : files)
    {
        const auto res = test_runner{settings}.run_from_file(f);

        total.passed += res.passed;
        total.failed += res.failed;
        total.skipped += res.skipped;
    }

    log_total(path, total);
    return total.failed == 0;
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        std::string target;
        test_settings settings;

        for (auto i = 1; i < argc; ++i)
        {
            if (argv[i][0] == '-')
            {
                if (argv[i] == std::string{"--skip-validation"})
                    settings.skip_validation = true;
                else if (argv[i] == std::string{"--hide-failed"})
                    settings.show_failed = false;
                else if (argv[i] == std::string{"--show-passed"})
                    settings.show_passed = true;
                else if (argv[i] == std::string{"--show-skipped"})
                    settings.show_skipped = true;
                else
                {
                    std::cerr << "Unknown argument: " << argv[i] << "\n";
                    return -1;
                }
            }
            else
                target = argv[i];
        }

        if (target.empty())
        {
            std::cerr << "Missing PATH argument\n";
            return -1;
        }

        const fs::path path = target;
        const bool res = fs::is_directory(path) ? run_tests_from_dir(path, settings) :
                                                  run_tests_from_file(path, settings);
        return res ? 0 : 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return -2;
    }
}
