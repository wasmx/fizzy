// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include <nlohmann/json.hpp>
#include <test/utils/hex.hpp>
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

// spectest module details:
// https://github.com/WebAssembly/spec/issues/1111
// https://github.com/WebAssembly/spec/blob/e3a3c12f51221daaa901691e674d7ff990029122/interpreter/host/spectest.ml#L33
// https://github.com/WebAssembly/spec/blob/f85f6e6b70fff9ee0f6ba21ebcdad2183bd67874/test/harness/sync_index.js#L79
/* wat2wasm
(module
  (func (export "print"))
  (func (export "print_i32") (param i32))
  (func (export "print_i32_f32") (param i32) (param f32))
  (func (export "print_f64_f64") (param f64) (param f64))
  (func (export "print_f32") (param f32))
  (func (export "print_f64") (param f64))
  (global (export "global_i32") i32 (i32.const 666))
  (global (export "global_f32") f32 (f32.const 666))
  (global (export "global_f64") f64 (f64.const 666))
  (table (export "table") 10 20 anyfunc)
  (memory (export "memory") 1 2)
)
*/
const auto spectest_bin = fizzy::test::from_hex(
    "0061736d01000000011a0660000060017f0060027f7d0060027c7c0060017d0060017c000307060001020304050405"
    "0170010a14050401010102061b037f00419a050b7d0043008026440b7c00440000000000d084400b0785010b057072"
    "696e740000097072696e745f69333200010d7072696e745f6933325f66333200020d7072696e745f6636345f663634"
    "0003097072696e745f6633320004097072696e745f66363400050a676c6f62616c5f69333203000a676c6f62616c5f"
    "66333203010a676c6f62616c5f6636340302057461626c650100066d656d6f727902000a130602000b02000b02000b"
    "02000b02000b02000b");
const auto spectest_module = fizzy::parse(spectest_bin);
const std::string spectest_name = "spectest";

template <typename T>
uint64_t json_to_value(const json& v)
{
    return static_cast<std::make_unsigned_t<T>>(std::stoull(v.get<std::string>()));
}

fizzy::bytes load_wasm_file(const fs::path& json_file_path, std::string_view filename)
{
    std::ifstream wasm_file{fs::path{json_file_path}.replace_filename(filename)};

    return fizzy::bytes(
        std::istreambuf_iterator<char>{wasm_file}, std::istreambuf_iterator<char>{});
}

struct test_settings
{
    bool skip_validation = false;
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
        m_instances[spectest_name] = fizzy::instantiate(spectest_module);
    }

    test_results run_from_file(const fs::path& path)
    {
        log("Running tests from " + path.string());

        std::ifstream test_file{path};
        const json j = json::parse(test_file);

        for (const auto& cmd : j.at("commands"))
        {
            const auto type = cmd.at("type").get<std::string>();

            log_no_newline("Line " + std::to_string(cmd.at("line").get<int>()) + ": " + type + " ");

            if (type == "module")
            {
                const auto filename = cmd.at("filename").get<std::string>();
                log_no_newline("Instantiating " + filename + " ");

                const std::string name =
                    (cmd.find("name") != cmd.end() ? cmd.at("name").get<std::string>() :
                                                     UnnamedModule);

                const auto wasm_binary = load_wasm_file(path, filename);
                try
                {
                    fizzy::Module module = fizzy::parse(wasm_binary);

                    auto [imports, error] = create_imports(module);
                    if (!error.empty())
                    {
                        fail(error);
                        m_instances.erase(name);
                        continue;
                    }

                    m_instances[name] = fizzy::instantiate(std::move(module),
                        std::move(imports.functions), std::move(imports.tables),
                        std::move(imports.memories), std::move(imports.globals));

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
                        pass();
                    else
                        fail(std::string{"Unexpected parser error: "} + ex.what());
                    continue;
                }
                catch (fizzy::validation_error const& ex)
                {
                    if (type == "assert_invalid")
                        pass();
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
                    fizzy::Module module = fizzy::parse(wasm_binary);

                    auto [imports, error] = create_imports(module);
                    if (!error.empty())
                    {
                        if (type == "assert_unlinkable")
                            pass();
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
                            pass();
                        else
                            fail(std::string{"Instantiation failed with error: "} + ex.what());
                    }
                    else
                    {
                        if (type == "assert_uninstantiable")
                            fail(std::string{"Instantiation failed with error: "} + ex.what());
                        else
                            pass();
                    }
                    continue;
                }

                fail("Module instantiated successfully. Expected error: " +
                     cmd.at("text").get<std::string>());
            }
            else
                skip("Unsupported command type");
        }

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

    std::optional<fizzy::ExecutionResult> invoke(const json& action)
    {
        auto instance = find_instance_for_action(action);
        if (!instance)
            return std::nullopt;

        const auto func_name = action.at("field").get<std::string>();
        const auto func_idx = fizzy::find_exported_function(instance->module, func_name);
        if (!func_idx.has_value())
        {
            skip("Function '" + func_name + "' not found.");
            return std::nullopt;
        }

        std::vector<fizzy::Value> args;
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
                skip("Unsupported argument type '" + arg_type + "'.");
                return std::nullopt;
            }
            args.push_back(arg_value);
        }

        try
        {
            return fizzy::execute(*instance, *func_idx, args);
        }
        catch (fizzy::unsupported_feature const& ex)
        {
            skip(std::string{"Unsupported feature: "} + ex.what());
            return std::nullopt;
        }
    }

    bool check_result(uint64_t actual_value, const json& expected)
    {
        const auto expected_type = expected.at("type").get<std::string>();
        uint64_t expected_value;
        if (expected_type == "i32")
            expected_value = json_to_value<int32_t>(expected.at("value"));
        else if (expected_type == "i64")
            expected_value = json_to_value<int64_t>(expected.at("value"));
        else
        {
            skip("Unsupported expected type '" + expected_type + "'.");
            return false;
        }

        if (expected_value != actual_value)
        {
            std::stringstream message;
            message << "Incorrect returned value. Expected: " << expected_value << " (0x"
                    << std::hex << expected_value << ") Actual: " << std::dec << actual_value
                    << " (0x" << std::hex << actual_value << std::dec << ")";
            fail(message.str());
            return false;
        }
        return true;
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

    void pass()
    {
        ++m_results.passed;
        std::cout << "PASSED\n";
    }

    void fail(std::string_view message)
    {
        ++m_results.failed;
        std::cout << "FAILED " << message << "\n";
    }

    void skip(std::string_view message)
    {
        ++m_results.skipped;
        std::cout << "SKIPPED " << message << "\n";
    }

    static void log(std::string_view message) { std::cout << message << "\n"; }

    static void log_no_newline(std::string_view message) { std::cout << message << std::flush; }

    test_settings m_settings;
    std::unordered_map<std::string, std::unique_ptr<fizzy::Instance>> m_instances;
    std::unordered_map<std::string, std::string> m_registered_names;
    std::string m_last_module_name;
    test_results m_results;
};

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

    std::cout << "TOTAL " << (total.passed + total.failed + total.skipped) << " tests ran from "
              << path << ".\n  PASSED " << total.passed << ", FAILED " << total.failed
              << ", SKIPPED " << total.skipped << ".\n";

    return total.failed == 0;
}

}  // namespace

int main(int argc, char** argv)
{
    try
    {
        std::string dir;
        test_settings settings;

        for (auto i = 1; i < argc; ++i)
        {
            if (argv[i][0] == '-')
            {
                if (argv[i] == std::string{"--skip-validation"})
                    settings.skip_validation = true;
                else
                {
                    std::cerr << "Unknown argument: " << argv[i] << "\n";
                    return -1;
                }
            }
            else
                dir = argv[i];
        }

        if (dir.empty())
        {
            std::cerr << "Missing DIR argument\n";
            return -1;
        }

        const bool res = run_tests_from_dir(dir, settings);
        return res ? 0 : 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return -2;
    }
}
