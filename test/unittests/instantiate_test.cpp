#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;


TEST(instantiate, imported_functions)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {FuncIdx{0}}});

    auto host_foo = [](Instance&, std::vector<uint64_t>) -> execution_result { return {true, {}}; };
    auto instance = instantiate(module, {{host_foo, module.typesec[0]}});

    ASSERT_EQ(instance->imported_functions.size(), 1);
    EXPECT_EQ(*instance->imported_functions[0].function.target<decltype(host_foo)>(), host_foo);
    ASSERT_EQ(instance->imported_functions[0].type.inputs.size(), 1);
    EXPECT_EQ(instance->imported_functions[0].type.inputs[0], ValType::i32);
    ASSERT_EQ(instance->imported_functions[0].type.outputs.size(), 1);
    EXPECT_EQ(instance->imported_functions[0].type.outputs[0], ValType::i32);
}

TEST(instantiate, imported_functions_multiple)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32}, {ValType::i32}});
    module.typesec.emplace_back(FuncType{{}, {}});
    module.importsec.emplace_back(Import{"mod", "foo1", ExternalKind::Function, {FuncIdx{0}}});
    module.importsec.emplace_back(Import{"mod", "foo2", ExternalKind::Function, {FuncIdx{1}}});

    auto host_foo1 = [](Instance&, std::vector<uint64_t>) -> execution_result {
        return {true, {0}};
    };
    auto host_foo2 = [](Instance&, std::vector<uint64_t>) -> execution_result {
        return {true, {}};
    };
    auto instance =
        instantiate(module, {{host_foo1, module.typesec[0]}, {host_foo2, module.typesec[1]}});

    ASSERT_EQ(instance->imported_functions.size(), 2);
    EXPECT_EQ(*instance->imported_functions[0].function.target<decltype(host_foo1)>(), host_foo1);
    ASSERT_EQ(instance->imported_functions[0].type.inputs.size(), 1);
    EXPECT_EQ(instance->imported_functions[0].type.inputs[0], ValType::i32);
    ASSERT_EQ(instance->imported_functions[0].type.outputs.size(), 1);
    EXPECT_EQ(instance->imported_functions[0].type.outputs[0], ValType::i32);
    EXPECT_EQ(*instance->imported_functions[1].function.target<decltype(host_foo2)>(), host_foo2);
    EXPECT_TRUE(instance->imported_functions[1].type.inputs.empty());
    EXPECT_TRUE(instance->imported_functions[1].type.outputs.empty());
}

TEST(instantiate, imported_functions_not_enough)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {FuncIdx{0}}});

    EXPECT_THROW_MESSAGE(instantiate(module, {}), instantiate_error,
        "Module requires 1 imported functions, 0 provided");
}

TEST(instantiate, imported_function_wrong_type)
{
    Module module;
    module.typesec.emplace_back(FuncType{{ValType::i32}, {ValType::i32}});
    module.importsec.emplace_back(Import{"mod", "foo", ExternalKind::Function, {FuncIdx{0}}});

    auto host_foo = [](Instance&, std::vector<uint64_t>) -> execution_result { return {true, {}}; };
    const auto host_foo_type = FuncType{{}, {}};

    ASSERT_THROW(instantiate(module, {{host_foo, host_foo_type}}), instantiate_error);
}

TEST(instantiate, imported_table)
{
    Module module;
    Import imp{"mod", "t", ExternalKind::Table, {}};
    imp.desc.table = Table{{10, 30}};
    module.importsec.emplace_back(imp);

    TableElements table;
    table.functions.resize(10);
    auto instance = instantiate(module, {}, {{&table, {10, 30}}});

    ASSERT_TRUE(instance->table);
    EXPECT_EQ(instance->table->functions.size(), 10);
    EXPECT_EQ(instance->table->functions.data(), table.functions.data());
}

TEST(instantiate, imported_table_stricter_limits)
{
    Module module;
    Import imp{"mod", "t", ExternalKind::Table, {}};
    imp.desc.table = Table{{10, 30}};
    module.importsec.emplace_back(imp);

    TableElements table;
    table.functions.resize(20);
    auto instance = instantiate(module, {}, {{&table, {20, 20}}});

    ASSERT_TRUE(instance->table);
    EXPECT_EQ(instance->table->functions.size(), 20);
    EXPECT_EQ(instance->table->functions.data(), table.functions.data());
}

TEST(instantiate, imported_table_invalid)
{
    Module module;
    Import imp{"mod", "t", ExternalKind::Table, {}};
    imp.desc.table = Table{{10, 30}};
    module.importsec.emplace_back(imp);

    TableElements table;
    table.functions.resize(10);

    // Providing more than 1 table
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table, {10, 30}}, {&table, {10, 10}}}),
        instantiate_error, "Only 1 imported table is allowed.");

    // Providing table when none expected
    Module module_no_imported_table;
    EXPECT_THROW_MESSAGE(instantiate(module_no_imported_table, {}, {{&table, {10, 30}}}),
        instantiate_error, "Trying to provide imported table to a module that doesn't define one.");

    // Not providing table when one is expected
    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "Module defines an imported table but none was provided.");

    // Provided min too low
    TableElements table_empty;
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table_empty, {0, 3}}}), instantiate_error,
        "Provided import's min is below import's min defined in module.");

    // Provided max too high
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table, {10, 40}}}), instantiate_error,
        "Provided import's max is above import's max defined in module.");

    // Provided max is unlimited
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table, {10, std::nullopt}}}), instantiate_error,
        "Provided import's max is above import's max defined in module.");

    // Null pointer
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{nullptr, {10, 30}}}), instantiate_error,
        "Provided imported table has a null pointer to data.");

    // Allocated less than min
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table_empty, {10, 30}}}), instantiate_error,
        "Provided imported table doesn't fit provided limits");

    // Allocated more than max
    TableElements table_big;
    table_big.functions.resize(40, /*0*/ std::nullopt);
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table_big, {10, 30}}}), instantiate_error,
        "Provided imported table doesn't fit provided limits");

    // Imported table and regular table
    Module module_with_two_tables;
    module_with_two_tables.tablesec.emplace_back(Table{{10, 10}});
    module_with_two_tables.importsec.emplace_back(imp);
    EXPECT_THROW_MESSAGE(instantiate(module_with_two_tables, {}, {{&table, {10, 30}}}),
        instantiate_error, "Cannot support more than 1 table section.");
}

TEST(instantiate, imported_memory)
{
    Module module;
    Import imp{"mod", "m", ExternalKind::Memory, {}};
    imp.desc.memory = Memory{{1, 3}};
    module.importsec.emplace_back(imp);

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, 3}}});

    ASSERT_TRUE(instance->memory);
    EXPECT_EQ(instance->memory->size(), PageSize);
    EXPECT_EQ(instance->memory->data(), memory.data());
    EXPECT_EQ(instance->memory_max_pages, 3);
}

TEST(instantiate, imported_memory_unlimited)
{
    Module module;
    Import imp{"mod", "m", ExternalKind::Memory, {}};
    imp.desc.memory = Memory{{1, std::nullopt}};
    module.importsec.emplace_back(imp);

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, std::nullopt}}});

    ASSERT_TRUE(instance->memory);
    EXPECT_EQ(instance->memory->size(), PageSize);
    EXPECT_EQ(instance->memory->data(), memory.data());
    EXPECT_EQ(instance->memory_max_pages, MemoryPagesLimit);
}

TEST(instantiate, imported_memory_stricter_limits)
{
    Module module;
    Import imp{"mod", "m", ExternalKind::Memory, {}};
    imp.desc.memory = Memory{{1, 3}};
    module.importsec.emplace_back(imp);

    bytes memory(PageSize * 2, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {2, 2}}});

    ASSERT_TRUE(instance->memory);
    EXPECT_EQ(instance->memory->size(), PageSize * 2);
    EXPECT_EQ(instance->memory->data(), memory.data());
    EXPECT_EQ(instance->memory_max_pages, 2);
}

TEST(instantiate, imported_memory_invalid)
{
    Module module;
    Import imp{"mod", "m", ExternalKind::Memory, {}};
    imp.desc.memory = Memory{{1, 3}};
    module.importsec.emplace_back(imp);

    bytes memory(PageSize, 0);

    // Providing more than 1 memory
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory, {1, 3}}, {&memory, {1, 1}}}),
        instantiate_error, "Only 1 imported memory is allowed.");

    // Providing memory when none expected
    Module module_no_imported_memory;
    EXPECT_THROW_MESSAGE(instantiate(module_no_imported_memory, {}, {}, {{&memory, {1, 3}}}),
        instantiate_error,
        "Trying to provide imported memory to a module that doesn't define one.");

    // Not providing memory when one is expected
    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "Module defines an imported memory but none was provided.");

    // Provided min too low
    bytes memory_empty;
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory_empty, {0, 3}}}), instantiate_error,
        "Provided import's min is below import's min defined in module.");

    // Provided max too high
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory, {1, 4}}}), instantiate_error,
        "Provided import's max is above import's max defined in module.");

    // Provided max is unlimited
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory, {1, std::nullopt}}}),
        instantiate_error, "Provided import's max is above import's max defined in module.");

    // Null pointer
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{nullptr, {1, 3}}}), instantiate_error,
        "Provided imported memory has a null pointer to data.");

    // Allocated less than min
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory_empty, {1, 3}}}), instantiate_error,
        "Provided imported memory doesn't fit provided limits");

    // Allocated more than max
    bytes memory_big(PageSize * 4, 0);
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory_big, {1, 3}}}), instantiate_error,
        "Provided imported memory doesn't fit provided limits");

    // Provided max exceeds the hard limit
    Module module_without_max;
    Import imp_without_max{"mod", "m", ExternalKind::Memory, {}};
    imp_without_max.desc.memory = Memory{{1, std::nullopt}};
    module_without_max.importsec.emplace_back(imp_without_max);
    EXPECT_THROW_MESSAGE(
        instantiate(module_without_max, {}, {}, {{&memory, {1, MemoryPagesLimit + 1}}}),
        instantiate_error,
        "Imported memory limits cannot exceed hard memory limit of 268435456 bytes.");

    // Imported memory and regular memory
    Module module_with_two_memories;
    module_with_two_memories.memorysec.emplace_back(Memory{{1, 1}});
    module_with_two_memories.importsec.emplace_back(imp);
    EXPECT_THROW_MESSAGE(instantiate(module_with_two_memories, {}, {}, {{&memory, {1, 3}}}),
        instantiate_error, "Cannot support more than 1 memory section.");
}

TEST(instantiate, imported_globals)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g", ExternalKind::Global, {true}});

    uint64_t global_value = 42;
    ExternalGlobal g{&global_value, true};
    auto instance = instantiate(module, {}, {}, {}, {g});

    ASSERT_EQ(instance->imported_globals.size(), 1);
    EXPECT_EQ(instance->imported_globals[0].is_mutable, true);
    EXPECT_EQ(*instance->imported_globals[0].value, 42);
    ASSERT_EQ(instance->globals.size(), 0);
}

TEST(instantiate, imported_globals_multiple)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {true}});
    module.importsec.emplace_back(Import{"mod", "g2", ExternalKind::Global, {false}});

    uint64_t global_value1 = 42;
    ExternalGlobal g1{&global_value1, true};
    uint64_t global_value2 = 43;
    ExternalGlobal g2{&global_value2, false};
    auto instance = instantiate(module, {}, {}, {}, {g1, g2});

    ASSERT_EQ(instance->imported_globals.size(), 2);
    EXPECT_EQ(instance->imported_globals[0].is_mutable, true);
    EXPECT_EQ(instance->imported_globals[1].is_mutable, false);
    EXPECT_EQ(*instance->imported_globals[0].value, 42);
    EXPECT_EQ(*instance->imported_globals[1].value, 43);
    ASSERT_EQ(instance->globals.size(), 0);
}

TEST(instantiate, imported_globals_mismatched_count)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {true}});
    module.importsec.emplace_back(Import{"mod", "g2", ExternalKind::Global, {false}});

    uint64_t global_value = 42;
    ExternalGlobal g{&global_value, true};
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {}, {g}), instantiate_error,
        "Module requires 2 imported globals, 1 provided");
}

TEST(instantiate, imported_globals_mismatched_mutability)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {true}});
    module.importsec.emplace_back(Import{"mod", "g2", ExternalKind::Global, {false}});

    uint64_t global_value1 = 42;
    ExternalGlobal g1{&global_value1, false};
    uint64_t global_value2 = 42;
    ExternalGlobal g2{&global_value2, true};
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {}, {g1, g2}), instantiate_error,
        "Global 0 mutability doesn't match module's global mutability");
}

TEST(instantiate, imported_globals_nullptr)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {false}});
    module.importsec.emplace_back(Import{"mod", "g2", ExternalKind::Global, {false}});

    ExternalGlobal g{nullptr, false};
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {}, {g, g}), instantiate_error,
        "Global 0 has a null pointer to value");
}

TEST(instantiate, memory_default)
{
    Module module;

    auto instance = instantiate(module);

    EXPECT_FALSE(instance->memory);
}

TEST(instantiate, memory_single)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});

    auto instance = instantiate(module);

    ASSERT_EQ(instance->memory->size(), PageSize);
    EXPECT_EQ(instance->memory_max_pages, 1);
}

TEST(instantiate, memory_single_unspecified_maximum)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, std::nullopt}});

    auto instance = instantiate(module);

    ASSERT_EQ(instance->memory->size(), PageSize);
    EXPECT_EQ(instance->memory_max_pages * PageSize, 256 * 1024 * 1024);
}

TEST(instantiate, memory_single_large_minimum)
{
    Module module;
    module.memorysec.emplace_back(Memory{{(1024 * 1024 * 1024) / PageSize, std::nullopt}});

    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "Cannot exceed hard memory limit of 268435456 bytes.");
}

TEST(instantiate, memory_single_large_maximum)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, (1024 * 1024 * 1024) / PageSize}});

    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "Cannot exceed hard memory limit of 268435456 bytes.");
}

TEST(instantiate, memory_multiple)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.memorysec.emplace_back(Memory{{1, 1}});

    EXPECT_THROW_MESSAGE(
        instantiate(module), instantiate_error, "Cannot support more than 1 memory section.");
}

TEST(instantiate, element_section)
{
    /* wat2wasm
    (module
      (table (export "tab") 4 funcref)
      (elem (i32.const 1) $f1 $f2) ;; Table contents: uninit, f1, f2, uninit
      (elem (i32.const 2) $f3 $f3) ;; Table contents: uninit, f1, f3, f3
      (func $f1 (result i32) (i32.const 1))
      (func $f2 (result i32) (i32.const 2))
      (func $f3 (result i32) (i32.const 3))
    )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f030403000000040401700004070701037461620100090f020041010b0200"
        "010041020b0202020a1003040041010b040041020b040041030b");

    auto instance = instantiate(parse(bin));

    ASSERT_EQ(instance->table->functions.size(), 4);
    EXPECT_FALSE(instance->table->functions[0].has_value());

    auto call_table_fn = [&instance](size_t idx) {
        auto& elem = instance->table->functions[idx];
        auto res = elem->function(*instance, {});
        return res.stack.front();
    };

    EXPECT_EQ(call_table_fn(1), 1);
    EXPECT_EQ(call_table_fn(2), 3);
    EXPECT_EQ(call_table_fn(3), 3);
}

TEST(instantiate, element_section_offset_from_global)
{
    // Manually generated binary,
    // because wabt doesn't support offset initied from non-imported global
    /*
    (module
      (table (export "tab") 4 funcref)
      (elem (global.get $offset) $f1 $f2) ;; Table contents: uninit, f1, f2, uninit
      (func $f1 (result i32) (i32.const 1))
      (func $f2 (result i32) (i32.const 2))
    )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f03030200000404017000040606017f0041010b0707010374616201"
        "000908010023000b0200010a0b02040041010b040041020b");
    auto instance = instantiate(parse(bin));

    ASSERT_EQ(instance->table->functions.size(), 4);
    EXPECT_FALSE(instance->table->functions[0].has_value());

    auto call_table_fn = [&instance](size_t idx) {
        auto& elem = instance->table->functions[idx];
        auto res = elem->function(*instance, {});
        return res.stack.front();
    };

    EXPECT_EQ(call_table_fn(1), 1);
    EXPECT_EQ(call_table_fn(2), 2);
    EXPECT_FALSE(instance->table->functions[3].has_value());
}

TEST(instantiate, element_section_offset_from_imported_global)
{
    /* wat2wasm
    (module
      (global (import "m" "g1") i32)
      (table (export "tab") 4 funcref)
      (elem (global.get 0) $f1 $f2) ;; Table contents: uninit, f1, f2, uninit
      (func $f1 (result i32) (i32.const 1))
      (func $f2 (result i32) (i32.const 2))
    )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f020901016d026731037f0003030200000404017000040707010374616201"
        "000908010023000b0200010a0b02040041010b040041020b");

    uint64_t global_value = 1;
    ExternalGlobal g{&global_value, false};

    auto instance = instantiate(parse(bin), {}, {}, {}, {g});

    ASSERT_EQ(instance->table->functions.size(), 4);
    EXPECT_FALSE(instance->table->functions[0].has_value());

    auto call_table_fn = [&instance](size_t idx) {
        auto& elem = instance->table->functions[idx];
        auto res = elem->function(*instance, {});
        return res.stack.front();
    };

    EXPECT_EQ(call_table_fn(1), 1);
    EXPECT_EQ(call_table_fn(2), 2);
    EXPECT_FALSE(instance->table->functions[3].has_value());
}

TEST(instantiate, element_section_offset_from_mutable_global)
{
    Module module;
    module.tablesec.emplace_back(Table{{4, std::nullopt}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    // Table contents: 0, 0xaa, 0xff, 0, ...
    module.elementsec.emplace_back(
        Element{{ConstantExpression::Kind::GlobalGet, {0}}, {0xaa, 0xff}});

    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "Constant expression can use global_get only for const globals.");
}

TEST(instantiate, element_section_offset_too_large)
{
    Module module;
    module.tablesec.emplace_back(Table{{3, std::nullopt}});
    module.elementsec.emplace_back(
        Element{{ConstantExpression::Kind::Constant, {1}}, {0xaa, 0xff}});
    module.elementsec.emplace_back(
        Element{{ConstantExpression::Kind::Constant, {2}}, {0x55, 0x55}});

    EXPECT_THROW_MESSAGE(
        instantiate(module), instantiate_error, "Element segment is out of table bounds");
}

TEST(instantiate, element_section_fills_imported_table)
{
    /* wat2wasm
    (module
      (table (import "m" "tab") 4 funcref)
      (elem (i32.const 1) $f1 $f2) ;; Table contents: uninit, f1, f2, uninit
      (elem (i32.const 2) $f3 $f4) ;; Table contents: uninit, f1, f3, f4
      (func $f1 (result i32) (i32.const 1))
      (func $f2 (result i32) (i32.const 2))
      (func $f3 (result i32) (i32.const 3))
      (func $f4 (result i32) (i32.const 4))
    )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f020b01016d037461620170000403050400000000090f020041010b020001"
        "0041020b0202030a1504040041010b040041020b040041030b040041040b");


    auto f0 = [](Instance&, std::vector<uint64_t>) { return execution_result{false, {0}}; };

    TableElements table;
    table.functions.resize(4);
    table.functions[0] = ExternalFunction{f0, FuncType{{}, {ValType::i32}}};
    auto instance = instantiate(parse(bin), {}, {{&table, {4, std::nullopt}}});

    ASSERT_EQ(instance->table->functions.size(), 4);

    auto call_table_fn = [&instance](size_t idx) {
        auto& elem = instance->table->functions[idx];
        auto res = elem->function(*instance, {});
        return res.stack.front();
    };

    EXPECT_EQ(call_table_fn(0), 0);
    EXPECT_EQ(call_table_fn(1), 1);
    EXPECT_EQ(call_table_fn(2), 3);
    EXPECT_EQ(call_table_fn(3), 4);
}

TEST(instantiate, data_section)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    // Memory contents: 0, 0xaa, 0xff, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::Constant, {1}}, {0xaa, 0xff}});
    // Memory contents: 0, 0xaa, 0x55, 0x55, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::Constant, {2}}, {0x55, 0x55}});

    auto instance = instantiate(module);

    EXPECT_EQ(instance->memory->substr(0, 6), from_hex("00aa55550000"));
}

TEST(instantiate, data_section_offset_from_global)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.globalsec.emplace_back(Global{false, {ConstantExpression::Kind::Constant, {42}}});
    // Memory contents: 0, 0xaa, 0xff, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::GlobalGet, {0}}, {0xaa, 0xff}});

    auto instance = instantiate(module);

    EXPECT_EQ(instance->memory->substr(42, 2), "aaff"_bytes);
}

TEST(instantiate, data_section_offset_from_imported_global)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {false}});
    module.memorysec.emplace_back(Memory{{1, 1}});
    // Memory contents: 0, 0xaa, 0xff, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::GlobalGet, {0}}, {0xaa, 0xff}});

    uint64_t global_value = 42;
    ExternalGlobal g{&global_value, false};

    auto instance = instantiate(module, {}, {}, {}, {g});

    EXPECT_EQ(instance->memory->substr(42, 2), "aaff"_bytes);
}

TEST(instantiate, data_section_offset_from_mutable_global)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, 1}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    // Memory contents: 0, 0xaa, 0xff, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::GlobalGet, {0}}, {0xaa, 0xff}});

    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "Constant expression can use global_get only for const globals.");
}

TEST(instantiate, data_section_offset_too_large)
{
    Module module;
    module.memorysec.emplace_back(Memory{{0, 1}});
    // Memory contents: 0, 0xaa, 0xff, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::Constant, {1}}, {0xaa, 0xff}});

    EXPECT_THROW_MESSAGE(
        instantiate(module), instantiate_error, "Data segment is out of memory bounds");
}

TEST(instantiate, data_section_fills_imported_memory)
{
    Module module;
    Import imp{"mod", "m", ExternalKind::Memory, {}};
    imp.desc.memory = Memory{{1, 1}};
    module.importsec.emplace_back(imp);
    // Memory contents: 0, 0xaa, 0xff, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::Constant, {1}}, {0xaa, 0xff}});
    // Memory contents: 0, 0xaa, 0x55, 0x55, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::Constant, {2}}, {0x55, 0x55}});

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, 1}}});

    EXPECT_EQ(memory.substr(0, 6), from_hex("00aa55550000"));
}

TEST(instantiate, globals_single)
{
    Module module;
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});

    auto instance = instantiate(module);

    ASSERT_EQ(instance->globals.size(), 1);
    EXPECT_EQ(instance->globals[0], 42);
}

TEST(instantiate, globals_multiple)
{
    Module module;
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module.globalsec.emplace_back(Global{false, {ConstantExpression::Kind::Constant, {43}}});

    auto instance = instantiate(module);

    ASSERT_EQ(instance->globals.size(), 2);
    EXPECT_EQ(instance->globals[0], 42);
    EXPECT_EQ(instance->globals[1], 43);
}

TEST(instantiate, globals_with_imported)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {true}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module.globalsec.emplace_back(Global{false, {ConstantExpression::Kind::Constant, {43}}});

    uint64_t global_value = 41;
    ExternalGlobal g{&global_value, true};

    auto instance = instantiate(module, {}, {}, {}, {g});

    ASSERT_EQ(instance->imported_globals.size(), 1);
    EXPECT_EQ(*instance->imported_globals[0].value, 41);
    EXPECT_EQ(instance->imported_globals[0].is_mutable, true);
    ASSERT_EQ(instance->globals.size(), 2);
    EXPECT_EQ(instance->globals[0], 42);
    EXPECT_EQ(instance->globals[1], 43);
}

TEST(instantiate, globals_initialized_from_imported)
{
    Module module;
    module.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {false}});
    module.globalsec.emplace_back(Global{true, {ConstantExpression::Kind::GlobalGet, {0}}});

    uint64_t global_value = 42;
    ExternalGlobal g{&global_value, false};

    auto instance = instantiate(module, {}, {}, {}, {g});

    ASSERT_EQ(instance->globals.size(), 1);
    EXPECT_EQ(instance->globals[0], 42);

    // initializing from mutable global is not allowed
    Module module_invalid1;
    module_invalid1.importsec.emplace_back(Import{"mod", "g1", ExternalKind::Global, {true}});
    module_invalid1.globalsec.emplace_back(
        Global{true, {ConstantExpression::Kind::GlobalGet, {0}}});

    ExternalGlobal g_mutable{&global_value, true};

    EXPECT_THROW_MESSAGE(instantiate(module_invalid1, {}, {}, {}, {g_mutable}), instantiate_error,
        "Constant expression can use global_get only for const globals.");

    // initializing from non-imported global is not allowed
    Module module_invalid2;
    module_invalid2.globalsec.emplace_back(
        Global{true, {ConstantExpression::Kind::Constant, {42}}});
    module_invalid2.globalsec.emplace_back(
        Global{true, {ConstantExpression::Kind::GlobalGet, {0}}});

    EXPECT_THROW_MESSAGE(instantiate(module_invalid2, {}, {}), instantiate_error,
        "Global can be initialized by another const global only if it's imported.");
}

TEST(execute, start_unreachable)
{
    Module module;
    module.startfunc = FuncIdx{0};
    // TODO: add type section (once enforced)
    module.funcsec.emplace_back(FuncIdx{0});
    module.codesec.emplace_back(Code{0, {Instr::unreachable}, {}});

    EXPECT_THROW_MESSAGE(
        instantiate(module), instantiate_error, "Start function failed to execute");
}