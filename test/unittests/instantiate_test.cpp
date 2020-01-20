#include "execute.hpp"
#include "utils.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

namespace
{
constexpr unsigned page_size = 65536;
}

TEST(instantiate, imported_functions)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {0}});

    auto host_foo = [](Instance&, std::vector<uint64_t>) -> execution_result { return {true, {}}; };
    auto instance = instantiate(module, {host_foo}, {});

    ASSERT_EQ(instance.imported_functions.size(), 1);
    EXPECT_EQ(instance.imported_functions[0], host_foo);
    ASSERT_EQ(instance.imported_function_types.size(), 1);
    EXPECT_EQ(instance.imported_function_types[0], TypeIdx{0});
}

TEST(instantiate, imported_globals)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g", ExternalKind::Global, {true}});

    auto instance = instantiate(module, {}, {42});

    ASSERT_EQ(instance.imported_globals_mutability.size(), 1);
    EXPECT_EQ(instance.imported_globals_mutability[0], true);
    ASSERT_EQ(instance.globals.size(), 1);
    EXPECT_EQ(instance.globals[0], 42);
}

TEST(instantiate, imported_globals_multiple)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {true}});
    module.importsec.emplace_back(Import{"mod", "g2", ExternalKind::Global, {false}});

    auto instance = instantiate(module, {}, {42, 43});

    ASSERT_EQ(instance.imported_globals_mutability.size(), 2);
    EXPECT_EQ(instance.imported_globals_mutability[0], true);
    EXPECT_EQ(instance.imported_globals_mutability[1], false);
    ASSERT_EQ(instance.globals.size(), 2);
    EXPECT_EQ(instance.globals[0], 42);
    EXPECT_EQ(instance.globals[1], 43);
}

TEST(instantiate, imported_globals_not_enough)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {true}});
    module.importsec.emplace_back(Import{"mod", "g2", ExternalKind::Global, {false}});

    EXPECT_THROW(instantiate(module, {}, {42}), std::runtime_error);
}

TEST(instantiate, imported_functions_multiple)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32}, {ValType::i32}});
    module.typesec.emplace_back(FuncType{{}, {}});
    module.importsec.emplace_back(Import{"mod", "foo1", ExternalKind::Function, {0}});
    module.importsec.emplace_back(Import{"mod", "foo2", ExternalKind::Function, {1}});

    auto host_foo1 = [](Instance&, std::vector<uint64_t>) -> execution_result {
        return {true, {0}};
    };
    auto host_foo2 = [](Instance&, std::vector<uint64_t>) -> execution_result {
        return {true, {}};
    };
    auto instance = instantiate(module, {host_foo1, host_foo2}, {});

    ASSERT_EQ(instance.imported_functions.size(), 2);
    EXPECT_EQ(instance.imported_functions[0], host_foo1);
    EXPECT_EQ(instance.imported_functions[1], host_foo2);
    ASSERT_EQ(instance.imported_function_types.size(), 2);
    EXPECT_EQ(instance.imported_function_types[0], TypeIdx{0});
    EXPECT_EQ(instance.imported_function_types[1], TypeIdx{1});
}

TEST(instantiate, imported_functions_not_enough)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {0}});

    EXPECT_THROW(instantiate(module, {}, {}), std::runtime_error);
}

TEST(instantiate, memory_default)
{
    Module module;

    auto instance = instantiate(module, {}, {});

    ASSERT_EQ(instance.memory.size(), 0);
    EXPECT_EQ(instance.memory_max_pages * page_size, 256 * 1024 * 1024);
}

TEST(instantiate, memory_single)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});

    auto instance = instantiate(module, {}, {});

    ASSERT_EQ(instance.memory.size(), page_size);
    EXPECT_EQ(instance.memory_max_pages, 1);
}

TEST(instantiate, memory_single_unspecified_maximum)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, std::nullopt}});

    auto instance = instantiate(module, {}, {});

    ASSERT_EQ(instance.memory.size(), page_size);
    EXPECT_EQ(instance.memory_max_pages * page_size, 256 * 1024 * 1024);
}

TEST(instantiate, memory_single_large_minimum)
{
    Module module;
    module.memorysec.emplace_back(Memory{{(1024 * 1024 * 1024) / page_size, std::nullopt}});

    EXPECT_THROW(instantiate(module, {}, {}), std::runtime_error);
}

TEST(instantiate, memory_single_large_maximum)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, (1024 * 1024 * 1024) / page_size}});

    EXPECT_THROW(instantiate(module, {}, {}), std::runtime_error);
}

TEST(instantiate, memory_multiple)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.memorysec.emplace_back(Memory{{1, 1}});

    EXPECT_THROW(instantiate(module, {}, {}), std::runtime_error);
}

TEST(instantiate, data_section)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    // Memory contents: 0, 0xaa, 0xff, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::Constant, {1}}, {0xaa, 0xff}});
    // Memory contents: 0, 0xaa, 0x55, 0x55, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::Constant, {2}}, {0x55, 0x55}});

    auto instance = instantiate(module, {}, {});

    EXPECT_EQ(instance.memory.substr(0, 6), from_hex("00aa55550000"));
}

TEST(instantiate, globals_single)
{
    Module module;
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});

    auto instance = instantiate(module, {}, {});

    ASSERT_EQ(instance.globals.size(), 1);
    EXPECT_EQ(instance.globals[0], 42);
}

TEST(instantiate, globals_multiple)
{
    Module module;
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module.globalsec.emplace_back(Global{false, {ConstantExpression::Kind::Constant, {43}}});

    auto instance = instantiate(module, {}, {});

    ASSERT_EQ(instance.globals.size(), 2);
    EXPECT_EQ(instance.globals[0], 42);
    EXPECT_EQ(instance.globals[1], 43);
}

TEST(instantiate, globals_with_imported)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {true}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module.globalsec.emplace_back(Global{false, {ConstantExpression::Kind::Constant, {43}}});

    auto instance = instantiate(module, {}, {41});

    ASSERT_EQ(instance.globals.size(), 3);
    EXPECT_EQ(instance.globals[0], 41);
    EXPECT_EQ(instance.globals[1], 42);
    EXPECT_EQ(instance.globals[2], 43);
}

TEST(instantiate, globals_initialized_from_imported)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {false}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::GlobalGet, {0}}});

    auto instance = instantiate(module, {}, {42});

    ASSERT_EQ(instance.globals.size(), 2);
    EXPECT_EQ(instance.globals[0], 42);
    EXPECT_EQ(instance.globals[1], 42);

    // initializing from mutable global is not allowed
    Module module_invalid1;
    module_invalid1.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {true}});
    module_invalid1.globalsec.emplace_back(
        Global{true, {ConstantExpression::Kind::GlobalGet, {0}}});

    EXPECT_THROW(instantiate(module_invalid1, {}, {42}), std::runtime_error);

    // initializing from non-imported global is not allowed
    Module module_invalid2;
    module_invalid2.globalsec.emplace_back(
        Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module_invalid2.globalsec.emplace_back(
        Global{true, {ConstantExpression::Kind::GlobalGet, {0}}});

    EXPECT_THROW(instantiate(module_invalid2, {}, {}), std::runtime_error);
}
