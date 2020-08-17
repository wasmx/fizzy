// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "limits.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

namespace
{
uint64_t call_table_func(Instance& instance, size_t idx)
{
    const auto& elem = (*instance.table)[idx];
    const auto res = elem->function(instance, {}, 0);
    EXPECT_TRUE(res.has_value);
    return res.value;
}
}  // namespace

TEST(instantiate, imported_functions)
{
    /* wat2wasm
      (func (import "mod" "foo") (param i32) (result i32))
    */
    const auto bin = from_hex("0061736d0100000001060160017f017f020b01036d6f6403666f6f0000");
    const auto module = parse(bin);

    auto host_foo = [](Instance&, span<const Value>, int) { return Trap; };
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
    /* wat2wasm
      (func (import "mod" "foo1") (param i32) (result i32))
      (func (import "mod" "foo2"))
    */
    const auto bin = from_hex(
        "0061736d0100000001090260017f017f600000021702036d6f6404666f6f310000036d6f6404666f6f320001");
    const auto module = parse(bin);

    auto host_foo1 = [](Instance&, span<const Value>, int) { return Trap; };
    auto host_foo2 = [](Instance&, span<const Value>, int) { return Trap; };
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
    /* wat2wasm
      (func (import "mod" "foo") (param i32) (result i32))
    */
    const auto bin = from_hex("0061736d0100000001060160017f017f020b01036d6f6403666f6f0000");
    const auto module = parse(bin);

    EXPECT_THROW_MESSAGE(instantiate(module, {}), instantiate_error,
        "module requires 1 imported functions, 0 provided");
}

TEST(instantiate, imported_function_wrong_type)
{
    /* wat2wasm
      (func (import "mod" "foo") (param i32) (result i32))
    */
    const auto bin = from_hex("0061736d0100000001060160017f017f020b01036d6f6403666f6f0000");
    const auto module = parse(bin);

    auto host_foo = [](Instance&, span<const Value>, int) { return Trap; };
    const auto host_foo_type = FuncType{{}, {}};

    EXPECT_THROW_MESSAGE(instantiate(module, {{host_foo, host_foo_type}}), instantiate_error,
        "function 0 type doesn't match module's imported function type");
}

TEST(instantiate, imported_table)
{
    /* wat2wasm
      (table (import "m" "t") 10 30 funcref)
    */
    const auto bin = from_hex("0061736d01000000020a01016d01740170010a1e");
    const auto module = parse(bin);

    table_elements table(10);
    auto instance = instantiate(module, {}, {{&table, {10, 30}}});

    ASSERT_TRUE(instance->table);
    EXPECT_EQ(instance->table->size(), 10);
    EXPECT_EQ(instance->table->data(), table.data());
}

TEST(instantiate, imported_table_stricter_limits)
{
    /* wat2wasm
      (table (import "m" "t") 10 30 funcref)
    */
    const auto bin = from_hex("0061736d01000000020a01016d01740170010a1e");
    const auto module = parse(bin);

    table_elements table(20);
    auto instance = instantiate(module, {}, {{&table, {20, 20}}});

    ASSERT_TRUE(instance->table);
    EXPECT_EQ(instance->table->size(), 20);
    EXPECT_EQ(instance->table->data(), table.data());
}

TEST(instantiate, imported_table_invalid)
{
    /* wat2wasm
      (table (import "m" "t") 10 30 funcref)
    */
    const auto bin = from_hex("0061736d01000000020a01016d01740170010a1e");
    const auto module = parse(bin);

    table_elements table(10);

    // Providing more than 1 table
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table, {10, 30}}, {&table, {10, 10}}}),
        instantiate_error, "only 1 imported table is allowed");

    // Providing table when none expected
    /* wat2wasm
      (module)
    */
    const auto bin_no_imported_table = from_hex("0061736d01000000");
    const auto module_no_imported_table = parse(bin_no_imported_table);
    EXPECT_THROW_MESSAGE(instantiate(module_no_imported_table, {}, {{&table, {10, 30}}}),
        instantiate_error, "trying to provide imported table to a module that doesn't define one");

    // Not providing table when one is expected
    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "module defines an imported table but none was provided");

    // Provided min too low
    table_elements table_empty;
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table_empty, {0, 3}}}), instantiate_error,
        "provided import's min is below import's min defined in module");

    // Provided max too high
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table, {10, 40}}}), instantiate_error,
        "provided import's max is above import's max defined in module");

    // Provided max is unlimited
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table, {10, std::nullopt}}}), instantiate_error,
        "provided import's max is above import's max defined in module");

    // Null pointer
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{nullptr, {10, 30}}}), instantiate_error,
        "provided imported table has a null pointer to data");

    // Allocated less than min
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table_empty, {10, 30}}}), instantiate_error,
        "provided imported table doesn't fit provided limits");

    // Allocated more than max
    table_elements table_big(40);
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table_big, {10, 30}}}), instantiate_error,
        "provided imported table doesn't fit provided limits");
}

TEST(instantiate, imported_memory)
{
    /* wat2wasm
      (memory (import "mod" "m") 1 3)
    */
    const auto bin = from_hex("0061736d01000000020b01036d6f64016d02010103");
    const auto module = parse(bin);

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, 3}}});

    ASSERT_TRUE(instance->memory);
    EXPECT_EQ(instance->memory->size(), PageSize);
    EXPECT_EQ(instance->memory->data(), memory.data());
    EXPECT_EQ(instance->memory_limits.min, 1);
    ASSERT_TRUE(instance->memory_limits.max.has_value());
    EXPECT_EQ(instance->memory_limits.max, 3);
}

TEST(instantiate, imported_memory_unlimited)
{
    /* wat2wasm
      (memory (import "mod" "m") 1)
    */
    const auto bin = from_hex("0061736d01000000020a01036d6f64016d020001");
    const auto module = parse(bin);

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, std::nullopt}}});

    ASSERT_TRUE(instance->memory);
    EXPECT_EQ(instance->memory->size(), PageSize);
    EXPECT_EQ(instance->memory->data(), memory.data());
    EXPECT_EQ(instance->memory_limits.min, 1);
    EXPECT_FALSE(instance->memory_limits.max.has_value());
}

TEST(instantiate, imported_memory_stricter_limits)
{
    /* wat2wasm
      (memory (import "mod" "m") 1 3)
    */
    const auto bin = from_hex("0061736d01000000020b01036d6f64016d02010103");
    const auto module = parse(bin);

    bytes memory(PageSize * 2, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {2, 2}}});

    ASSERT_TRUE(instance->memory);
    EXPECT_EQ(instance->memory->size(), PageSize * 2);
    EXPECT_EQ(instance->memory->data(), memory.data());
    EXPECT_EQ(instance->memory_limits.min, 2);
    ASSERT_TRUE(instance->memory_limits.max.has_value());
    EXPECT_EQ(instance->memory_limits.max, 2);
}

TEST(instantiate, imported_memory_invalid)
{
    /* wat2wasm
      (memory (import "mod" "m") 1 3)
    */
    const auto bin = from_hex("0061736d01000000020b01036d6f64016d02010103");
    const auto module = parse(bin);

    bytes memory(PageSize, 0);

    // Providing more than 1 memory
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory, {1, 3}}, {&memory, {1, 1}}}),
        instantiate_error, "only 1 imported memory is allowed");

    // Providing memory when none expected
    /* wat2wasm
      (module)
    */
    const auto bin_no_imported_memory = from_hex("0061736d01000000");
    const auto module_no_imported_memory = parse(bin_no_imported_memory);
    EXPECT_THROW_MESSAGE(instantiate(module_no_imported_memory, {}, {}, {{&memory, {1, 3}}}),
        instantiate_error, "trying to provide imported memory to a module that doesn't define one");

    // Not providing memory when one is expected
    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "module defines an imported memory but none was provided");

    // Provided min too low
    bytes memory_empty;
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory_empty, {0, 3}}}), instantiate_error,
        "provided import's min is below import's min defined in module");

    // Provided max too high
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory, {1, 4}}}), instantiate_error,
        "provided import's max is above import's max defined in module");

    // Provided max is unlimited
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory, {1, std::nullopt}}}),
        instantiate_error, "provided import's max is above import's max defined in module");

    // Null pointer
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{nullptr, {1, 3}}}), instantiate_error,
        "provided imported memory has a null pointer to data");

    // Allocated less than min
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory_empty, {1, 3}}}), instantiate_error,
        "provided imported memory doesn't fit provided limits");

    // Allocated more than max
    bytes memory_big(PageSize * 4, 0);
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory_big, {1, 3}}}), instantiate_error,
        "provided imported memory doesn't fit provided limits");

    // Provided max exceeds the hard limit
    /* wat2wasm
      (memory (import "mod" "m") 1)
    */
    const auto bin_without_max = from_hex("0061736d01000000020a01036d6f64016d020001");
    const auto module_without_max = parse(bin_without_max);
    EXPECT_THROW_MESSAGE(
        instantiate(module_without_max, {}, {}, {{&memory, {1, MemoryPagesLimit + 1}}}),
        instantiate_error,
        "imported memory limits cannot exceed hard memory limit of 268435456 bytes");
}

TEST(instantiate, imported_globals)
{
    /* wat2wasm
      (global (import "mod" "g") (mut i32))
    */
    const auto bin = from_hex("0061736d01000000020a01036d6f640167037f01");
    const auto module = parse(bin);

    Value global_value = 42;
    ExternalGlobal g{&global_value, {ValType::i32, true}};
    auto instance = instantiate(module, {}, {}, {}, {g});

    ASSERT_EQ(instance->imported_globals.size(), 1);
    EXPECT_EQ(instance->imported_globals[0].type.value_type, ValType::i32);
    EXPECT_TRUE(instance->imported_globals[0].type.is_mutable);
    EXPECT_EQ(*instance->imported_globals[0].value, 42);
    ASSERT_EQ(instance->globals.size(), 0);
}

TEST(instantiate, imported_globals_multiple)
{
    /* wat2wasm
      (global (import "mod" "g1") (mut i32))
      (global (import "mod" "g2") i32)
    */
    const auto bin = from_hex("0061736d01000000021502036d6f64026731037f01036d6f64026732037f00");
    const auto module = parse(bin);

    Value global_value1 = 42;
    ExternalGlobal g1{&global_value1, {ValType::i32, true}};
    Value global_value2 = 43;
    ExternalGlobal g2{&global_value2, {ValType::i32, false}};
    auto instance = instantiate(module, {}, {}, {}, {g1, g2});

    ASSERT_EQ(instance->imported_globals.size(), 2);
    EXPECT_EQ(instance->imported_globals[0].type.value_type, ValType::i32);
    EXPECT_TRUE(instance->imported_globals[0].type.is_mutable);
    EXPECT_EQ(instance->imported_globals[1].type.value_type, ValType::i32);
    EXPECT_FALSE(instance->imported_globals[1].type.is_mutable);
    EXPECT_EQ(*instance->imported_globals[0].value, 42);
    EXPECT_EQ(*instance->imported_globals[1].value, 43);
    ASSERT_EQ(instance->globals.size(), 0);
}

TEST(instantiate, imported_globals_mismatched_count)
{
    /* wat2wasm
      (global (import "mod" "g1") (mut i32))
      (global (import "mod" "g2") i32)
    */
    const auto bin = from_hex("0061736d01000000021502036d6f64026731037f01036d6f64026732037f00");
    const auto module = parse(bin);

    Value global_value = 42;
    ExternalGlobal g{&global_value, {ValType::i32, true}};
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {}, {g}), instantiate_error,
        "module requires 2 imported globals, 1 provided");
}

TEST(instantiate, imported_globals_mismatched_mutability)
{
    /* wat2wasm
      (global (import "mod" "g1") (mut i32))
      (global (import "mod" "g2") i32)
    */
    const auto bin = from_hex("0061736d01000000021502036d6f64026731037f01036d6f64026732037f00");
    const auto module = parse(bin);

    Value global_value1 = 42;
    ExternalGlobal g1{&global_value1, {ValType::i32, false}};
    Value global_value2 = 42;
    ExternalGlobal g2{&global_value2, {ValType::i32, true}};
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {}, {g1, g2}), instantiate_error,
        "global 0 mutability doesn't match module's global mutability");
}

TEST(instantiate, DISABLED_imported_globals_mismatched_type)
{
    /* wat2wasm
      (global (export "g1") i64 (i64.const 0))
    */
    const auto bin1 = from_hex("0061736d010000000606017e0042000b0706010267310300");
    auto instance1 = instantiate(parse(bin1));

    const auto g = find_exported_global(*instance1, "g1");
    ASSERT_TRUE(g.has_value());

    /* wat2wasm
      (global (import "mod" "g1") i32)
    */
    const auto bin2 = from_hex("0061736d01000000020b01036d6f64026731037f00");
    const auto module2 = parse(bin2);

    EXPECT_THROW_MESSAGE(
        instantiate(module2, {}, {}, {}, {*g}), instantiate_error, "type mismatch");
}

TEST(instantiate, imported_globals_nullptr)
{
    /* wat2wasm
      (global (import "mod" "g1") i32)
      (global (import "mod" "g2") i32)
    */
    const auto bin = from_hex("0061736d01000000021502036d6f64026731037f00036d6f64026732037f00");
    const auto module = parse(bin);

    ExternalGlobal g{nullptr, {ValType::i32, false}};
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {}, {g, g}), instantiate_error,
        "global 0 has a null pointer to value");
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
    EXPECT_EQ(instance->memory_limits.min, 1);
    ASSERT_TRUE(instance->memory_limits.max.has_value());
    EXPECT_EQ(instance->memory_limits.max, 1);
}

TEST(instantiate, memory_single_unspecified_maximum)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, std::nullopt}});

    auto instance = instantiate(module);

    ASSERT_EQ(instance->memory->size(), PageSize);
    EXPECT_EQ(instance->memory_limits.min, 1);
    EXPECT_FALSE(instance->memory_limits.max.has_value());
}

TEST(instantiate, memory_single_large_minimum)
{
    Module module;
    module.memorysec.emplace_back(Memory{{(1024 * 1024 * 1024) / PageSize, std::nullopt}});

    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "cannot exceed hard memory limit of 268435456 bytes");
}

TEST(instantiate, memory_single_large_maximum)
{
    Module module;
    module.memorysec.emplace_back(Memory{{1, (1024 * 1024 * 1024) / PageSize}});

    EXPECT_THROW_MESSAGE(instantiate(module), instantiate_error,
        "cannot exceed hard memory limit of 268435456 bytes");
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

    ASSERT_EQ(instance->table->size(), 4);
    EXPECT_FALSE((*instance->table)[0].has_value());
    EXPECT_EQ(call_table_func(*instance, 1), 1);
    EXPECT_EQ(call_table_func(*instance, 2), 3);
    EXPECT_EQ(call_table_func(*instance, 3), 3);
}

TEST(instantiate, element_section_offset_from_global)
{
    // Manually generated binary,
    // because wabt doesn't support offset initied from non-imported global
    /*
    (module
      (global $offset i32 (i32.const 1))
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

    ASSERT_EQ(instance->table->size(), 4);
    EXPECT_FALSE((*instance->table)[0].has_value());
    EXPECT_EQ(call_table_func(*instance, 1), 1);
    EXPECT_EQ(call_table_func(*instance, 2), 2);
    EXPECT_FALSE((*instance->table)[3].has_value());
}

TEST(instantiate, element_section_offset_from_imported_global)
{
    /* wat2wasm
      (global (import "mod" "g") i32)
      (table 4 funcref)
      (elem (global.get 0) 0 1)
      (func (result i32) (i32.const 1))
      (func (result i32) (i32.const 2))
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f020a01036d6f640167037f0003030200000404017000040908010023000b"
        "0200010a0b02040041010b040041020b");

    Value global_value = 1;
    ExternalGlobal g{&global_value, {ValType::i32, false}};

    auto instance = instantiate(parse(bin), {}, {}, {}, {g});

    ASSERT_EQ(instance->table->size(), 4);
    EXPECT_FALSE((*instance->table)[0].has_value());
    EXPECT_EQ(call_table_func(*instance, 1), 1);
    EXPECT_EQ(call_table_func(*instance, 2), 2);
    EXPECT_FALSE((*instance->table)[3].has_value());
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
        instantiate(module), instantiate_error, "element segment is out of table bounds");
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

    auto f0 = [](Instance&, span<const Value>, int) { return Value{0}; };

    table_elements table(4);
    table[0] = ExternalFunction{f0, FuncType{{}, {ValType::i32}}};
    auto instance = instantiate(parse(bin), {}, {{&table, {4, std::nullopt}}});

    ASSERT_EQ(instance->table->size(), 4);
    EXPECT_EQ(call_table_func(*instance, 0), 0);
    EXPECT_EQ(call_table_func(*instance, 1), 1);
    EXPECT_EQ(call_table_func(*instance, 2), 3);
    EXPECT_EQ(call_table_func(*instance, 3), 4);
}

TEST(instantiate, element_section_out_of_bounds_doesnt_change_imported_table)
{
    /* wat2wasm
    (module
      (table (import "m" "tab") 3 funcref)
      (elem (i32.const 0) $f1 $f1)
      (elem (i32.const 2) $f1 $f1)
      (func $f1 (result i32) (i32.const 1))
    )
    */
    const auto bin = from_hex(
        "0061736d010000000105016000017f020b01016d037461620170000303020100090f020041000b020000004102"
        "0b0200000a0601040041010b");
    Module module = parse(bin);

    auto f0 = [](Instance&, span<const Value>, int) { return Value{0}; };

    table_elements table(3);
    table[0] = ExternalFunction{f0, FuncType{{}, {ValType::i32}}};

    EXPECT_THROW_MESSAGE(instantiate(module, {}, {{&table, {3, std::nullopt}}}), instantiate_error,
        "element segment is out of table bounds");

    ASSERT_EQ(table.size(), 3);
    EXPECT_EQ(*table[0]->function.target<decltype(f0)>(), f0);
    EXPECT_FALSE(table[1].has_value());
    EXPECT_FALSE(table[2].has_value());
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
    module.globalsec.emplace_back(
        Global{{ValType::i32, false}, {ConstantExpression::Kind::Constant, {42}}});
    // Memory contents: 0, 0xaa, 0xff, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::GlobalGet, {0}}, {0xaa, 0xff}});

    auto instance = instantiate(module);

    EXPECT_EQ(instance->memory->substr(42, 2), "aaff"_bytes);
}

TEST(instantiate, data_section_offset_from_imported_global)
{
    /* wat2wasm
      (global (import "mod" "g") i32)
      (memory 1 1)
      (data (global.get 0) "\aa\ff")
    */
    const auto bin =
        from_hex("0061736d01000000020a01036d6f640167037f000504010101010b08010023000b02aaff");
    const auto module = parse(bin);

    Value global_value = 42;
    ExternalGlobal g{&global_value, {ValType::i32, false}};

    auto instance = instantiate(module, {}, {}, {}, {g});

    EXPECT_EQ(instance->memory->substr(42, 2), "aaff"_bytes);
}

TEST(instantiate, data_section_offset_too_large)
{
    Module module;
    module.memorysec.emplace_back(Memory{{0, 1}});
    // Memory contents: 0, 0xaa, 0xff, 0, ...
    module.datasec.emplace_back(Data{{ConstantExpression::Kind::Constant, {1}}, {0xaa, 0xff}});

    EXPECT_THROW_MESSAGE(
        instantiate(module), instantiate_error, "data segment is out of memory bounds");
}

TEST(instantiate, data_section_fills_imported_memory)
{
    /* wat2wasm
      (memory (import "mod" "m") 1 1)
      (data (i32.const 1) "\aa\ff") ;; Memory contents: 0, 0xaa, 0xff, 0, ...
      (data (i32.const 2) "\55\55") ;; Memory contents: 0, 0xaa, 0x55, 0x55, 0, ...
    */
    const auto bin =
        from_hex("0061736d01000000020b01036d6f64016d020101010b0f020041010b02aaff0041020b025555");
    const auto module = parse(bin);

    bytes memory(PageSize, 0);
    auto instance = instantiate(module, {}, {}, {{&memory, {1, 1}}});

    EXPECT_EQ(memory.substr(0, 6), from_hex("00aa55550000"));
}

TEST(instantiate, data_section_out_of_bounds_doesnt_change_imported_memory)
{
    /* wat2wasm
    (module
      (memory (import "m" "mem") 1)
      (data (i32.const 0) "a")
      (data (i32.const 65536) "a")
    )
    */
    const auto bin =
        from_hex("0061736d01000000020a01016d036d656d0200010b0f020041000b016100418080040b0161");
    Module module = parse(bin);

    bytes memory(PageSize, 0);
    EXPECT_THROW_MESSAGE(instantiate(module, {}, {}, {{&memory, {1, 1}}}), instantiate_error,
        "data segment is out of memory bounds");

    EXPECT_EQ(memory[0], 0);
}

TEST(instantiate, data_elem_section_errors_dont_change_imports)
{
    /* wat2wasm
    (module
      (table (import "m" "tab") 3 funcref)
      (memory (import "m" "mem") 1)
      (elem (i32.const 0) $f1 $f1)
      (data (i32.const 0) "a")
      (data (i32.const 65536) "a")
      (func $f1 (result i32) (i32.const 1))
    )
    */
    const auto bin_data_error = from_hex(
        "0061736d010000000105016000017f021402016d0374616201700003016d036d656d0200010302010009080100"
        "41000b0200000a0601040041010b0b0f020041000b016100418080040b0161");
    Module module_data_error = parse(bin_data_error);

    table_elements table(3);
    bytes memory(PageSize, 0);
    EXPECT_THROW_MESSAGE(
        instantiate(module_data_error, {}, {{&table, {3, std::nullopt}}}, {{&memory, {1, 1}}}),
        instantiate_error, "data segment is out of memory bounds");

    EXPECT_FALSE(table[0].has_value());
    EXPECT_FALSE(table[1].has_value());
    EXPECT_EQ(memory[0], 0);

    /* wat2wasm
    (module
      (table (import "m" "tab") 3 funcref)
      (memory (import "m" "mem") 1)
      (elem (i32.const 0) $f1 $f1)
      (elem (i32.const 2) $f1 $f1)
      (data (i32.const 0) "a")
      (func $f1 (result i32) (i32.const 1))
    )
    */
    const auto bin_elem_error = from_hex(
        "0061736d010000000105016000017f021402016d0374616201700003016d036d656d02000103020100090f0200"
        "41000b0200000041020b0200000a0601040041010b0b07010041000b0161");
    Module module_elem_error = parse(bin_elem_error);

    EXPECT_THROW_MESSAGE(
        instantiate(module_elem_error, {}, {{&table, {3, std::nullopt}}}, {{&memory, {1, 1}}}),
        instantiate_error, "element segment is out of table bounds");

    EXPECT_FALSE(table[0].has_value());
    EXPECT_FALSE(table[1].has_value());
    EXPECT_FALSE(table[2].has_value());
    EXPECT_EQ(memory[0], 0);
}

TEST(instantiate, globals_single)
{
    Module module;
    module.globalsec.emplace_back(
        Global{{ValType::i32, true}, {ConstantExpression::Kind::Constant, {42}}});

    auto instance = instantiate(module);

    ASSERT_EQ(instance->globals.size(), 1);
    EXPECT_EQ(instance->globals[0], 42);
}

TEST(instantiate, globals_multiple)
{
    Module module;
    module.globalsec.emplace_back(
        Global{{ValType::i32, true}, {ConstantExpression::Kind::Constant, {42}}});
    module.globalsec.emplace_back(
        Global{{ValType::i32, false}, {ConstantExpression::Kind::Constant, {43}}});

    auto instance = instantiate(module);

    ASSERT_EQ(instance->globals.size(), 2);
    EXPECT_EQ(instance->globals[0], 42);
    EXPECT_EQ(instance->globals[1], 43);
}

TEST(instantiate, globals_with_imported)
{
    /* wat2wasm
      (global (import "mod" "g1") (mut i32))
      (global (mut i32) (i32.const 42))
      (global i32 (i32.const 43))
    */
    const auto bin =
        from_hex("0061736d01000000020b01036d6f64026731037f01060b027f01412a0b7f00412b0b");
    const auto module = parse(bin);

    Value global_value = 41;
    ExternalGlobal g{&global_value, {ValType::i32, true}};

    auto instance = instantiate(module, {}, {}, {}, {g});

    ASSERT_EQ(instance->imported_globals.size(), 1);
    EXPECT_EQ(*instance->imported_globals[0].value, 41);
    EXPECT_EQ(instance->imported_globals[0].type.is_mutable, true);
    ASSERT_EQ(instance->globals.size(), 2);
    EXPECT_EQ(instance->globals[0], 42);
    EXPECT_EQ(instance->globals[1], 43);
}

TEST(instantiate, globals_initialized_from_imported)
{
    /* wat2wasm
      (global (import "mod" "g1") i32)
      (global (mut i32) (global.get 0))
    */
    const auto bin = from_hex("0061736d01000000020b01036d6f64026731037f000606017f0123000b");
    const auto module = parse(bin);

    Value global_value = 42;
    ExternalGlobal g{&global_value, {ValType::i32, false}};

    auto instance = instantiate(module, {}, {}, {}, {g});

    ASSERT_EQ(instance->globals.size(), 1);
    EXPECT_EQ(instance->globals[0], 42);
}

TEST(instantiate, globals_float)
{
    /* wat2wasm
      (global (import "m" "g1") (mut f32))
      (global (import "m" "g2") f64)
      (global f32 (f32.const 1.2))
      (global (mut f64) (f64.const 3.4))
      (global f64 (global.get 1))
    */
    const auto bin = from_hex(
        "0061736d01000000021102016d026731037d01016d026732037c00061a037d00439a99993f0b7c014433333333"
        "33330b400b7c0023010b");
    const auto module = parse(bin);

    Value global_value1 = 5.6f;
    ExternalGlobal g1{&global_value1, {ValType::f32, true}};
    Value global_value2 = 7.8;
    ExternalGlobal g2{&global_value2, {ValType::f64, false}};

    auto instance = instantiate(module, {}, {}, {}, {g1, g2});

    ASSERT_EQ(instance->imported_globals.size(), 2);
    EXPECT_EQ(instance->imported_globals[0].value->f32, 5.6f);
    EXPECT_EQ(instance->imported_globals[0].type.value_type, ValType::f32);
    EXPECT_TRUE(instance->imported_globals[0].type.is_mutable);
    EXPECT_EQ(instance->imported_globals[1].value->f64, 7.8);
    EXPECT_EQ(instance->imported_globals[1].type.value_type, ValType::f64);
    EXPECT_FALSE(instance->imported_globals[1].type.is_mutable);
    ASSERT_EQ(instance->globals.size(), 3);
    EXPECT_EQ(instance->globals[0].f32, 1.2f);
    EXPECT_EQ(instance->globals[1].f64, 3.4);
    EXPECT_EQ(instance->globals[2].f64, 7.8);
}

TEST(instantiate, start_unreachable)
{
    /* wat2wasm
    (start 0)
    (func (unreachable))
    */
    const auto wasm = from_hex("0061736d01000000010401600000030201000801000a05010300000b");

    EXPECT_THROW_MESSAGE(
        instantiate(parse(wasm)), instantiate_error, "start function failed to execute");
}
