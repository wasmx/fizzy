#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <lib/fizzy/limits.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;

TEST(api, find_exported_function)
{
    Module module;
    module.exportsec.emplace_back(Export{"foo1", ExternalKind::Function, 0});
    module.exportsec.emplace_back(Export{"foo2", ExternalKind::Function, 1});
    module.exportsec.emplace_back(Export{"foo3", ExternalKind::Function, 2});
    module.exportsec.emplace_back(Export{"foo4", ExternalKind::Function, 42});
    module.exportsec.emplace_back(Export{"mem", ExternalKind::Memory, 0});
    module.exportsec.emplace_back(Export{"glob", ExternalKind::Global, 0});
    module.exportsec.emplace_back(Export{"table", ExternalKind::Table, 0});

    auto optionalIdx = find_exported_function(module, "foo1");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 0);

    optionalIdx = find_exported_function(module, "foo2");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 1);

    optionalIdx = find_exported_function(module, "foo3");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 2);

    optionalIdx = find_exported_function(module, "foo4");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 42);

    EXPECT_FALSE(find_exported_function(module, "foo5"));
    EXPECT_FALSE(find_exported_function(module, "mem"));
    EXPECT_FALSE(find_exported_function(module, "glob"));
    EXPECT_FALSE(find_exported_function(module, "table"));
}

TEST(api, find_exported_global)
{
    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (global (export "g1") (mut i32) (i32.const 0))
      (global (export "g2") i32 (i32.const 1))
      (global (export "g3") (mut i32) (i32.const 2))
      (global (export "g4") i32 (i32.const 3))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 0)
    )
     */
    const auto wasm = from_hex(
        "0061736d010000000104016000000302010004040170000005030100000615047f0141000b7f0041010b7f0141"
        "020b7f0041030b072507016600000267310300026732030102673303020267340303037461620100036d656d02"
        "000a05010300010b");

    auto instance = instantiate(parse(wasm));

    auto opt_global = find_exported_global(instance, "g1");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(*opt_global->value, 0);
    EXPECT_TRUE(opt_global->is_mutable);

    opt_global = find_exported_global(instance, "g2");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(*opt_global->value, 1);
    EXPECT_FALSE(opt_global->is_mutable);

    opt_global = find_exported_global(instance, "g3");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(*opt_global->value, 2);
    EXPECT_TRUE(opt_global->is_mutable);

    opt_global = find_exported_global(instance, "g4");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(*opt_global->value, 3);
    EXPECT_FALSE(opt_global->is_mutable);

    EXPECT_FALSE(find_exported_global(instance, "g5"));
    EXPECT_FALSE(find_exported_global(instance, "f"));
    EXPECT_FALSE(find_exported_global(instance, "tab"));
    EXPECT_FALSE(find_exported_global(instance, "mem"));

    /* wat2wasm
    (module
      (global (export "g1") (import "test" "g2") i32)
      (global (export "g2") (mut i32) (i32.const 1))
      (table (export "tab") 0 anyfunc)
      (func (export "f") nop)
      (memory (export "mem") 0)
    )
     */
    const auto wasm_reexported_global = from_hex(
        "0061736d01000000010401600000020c010474657374026732037f000302010004040170000005030100000606"
        "017f0141010b071b050267310300026732030103746162010001660000036d656d02000a05010300010b");

    uint64_t g1 = 42;
    auto instance_reexported_global =
        instantiate(parse(wasm_reexported_global), {}, {}, {}, {ExternalGlobal{&g1, false}});

    opt_global = find_exported_global(instance_reexported_global, "g1");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(opt_global->value, &g1);
    EXPECT_FALSE(opt_global->is_mutable);

    opt_global = find_exported_global(instance_reexported_global, "g2");
    ASSERT_TRUE(opt_global);
    EXPECT_EQ(*opt_global->value, 1);
    EXPECT_TRUE(opt_global->is_mutable);

    EXPECT_FALSE(find_exported_global(instance_reexported_global, "g3").has_value());

    /* wat2wasm
    (module
      (table (export "tab") 0 anyfunc)
      (func (export "f") nop)
      (memory (export "mem") 0)
    )
     */
    const auto wasm_no_globals = from_hex(
        "0061736d0100000001040160000003020100040401700000050301000007110303746162010001660000036d65"
        "6d02000a05010300010b");

    auto instance_no_globals = instantiate(parse(wasm_no_globals));

    EXPECT_FALSE(find_exported_global(instance_no_globals, "g1").has_value());
}

TEST(api, find_exported_table)
{
    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (func $g nop)
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 2 20 anyfunc)
      (elem 0 (i32.const 0) $g $f)
      (memory (export "mem") 0)
    )
     */
    const auto wasm = from_hex(
        "0061736d0100000001040160000003030200000405017001021405030100000606017f0041000b071604016600"
        "000267310300037461620100036d656d02000908010041000b0201000a09020300010b0300010b");

    auto instance = instantiate(parse(wasm));

    auto opt_table = find_exported_table(instance, "tab");
    ASSERT_TRUE(opt_table);
    EXPECT_EQ(opt_table->table, instance.table.get());
    EXPECT_EQ(opt_table->table->size(), 2);
    EXPECT_EQ((*opt_table->table)[0], 1);
    EXPECT_EQ((*opt_table->table)[1], 0);
    EXPECT_EQ(opt_table->limits.min, 2);
    ASSERT_TRUE(opt_table->limits.max.has_value());
    EXPECT_EQ(opt_table->limits.max, 20);

    EXPECT_FALSE(find_exported_table(instance, "ttt").has_value());

    /* wat2wasm
    (module
      (table (import "test" "table") 2 20 anyfunc)
      (export "tab" (table 0))
      (func $f (export "f") nop)
      (func $g nop)
      (global (export "g1") i32 (i32.const 0))
      (memory (export "mem") 0)
    )
     */
    const auto wasm_reexported_table = from_hex(
        "0061736d010000000104016000000211010474657374057461626c650170010214030302000005030100000606"
        "017f0041000b071604037461620100016600000267310300036d656d02000a09020300010b0300010b");

    std::vector<FuncIdx> table = {1, 0};
    auto instance_reexported_table =
        instantiate(parse(wasm_reexported_table), {}, {ExternalTable{&table, {2, 20}}});

    opt_table = find_exported_table(instance_reexported_table, "tab");
    ASSERT_TRUE(opt_table);
    EXPECT_EQ(opt_table->table, &table);
    EXPECT_EQ(opt_table->limits.min, 2);
    ASSERT_TRUE(opt_table->limits.max.has_value());
    EXPECT_EQ(opt_table->limits.max, 20);

    EXPECT_FALSE(find_exported_table(instance, "ttt").has_value());

    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (global (export "g1") i32 (i32.const 0))
      (memory (export "mem") 0)
    )
    */
    const auto wasm_no_table = from_hex(
        "0061736d010000000104016000000302010005030100000606017f0041000b071003016600000267310300036d"
        "656d02000a05010300010b");

    auto instance_no_table = instantiate(parse(wasm_no_table));

    EXPECT_FALSE(find_exported_table(instance_no_table, "tab").has_value());
}

TEST(api, DISABLED_find_exported_table_reimport)
{
    /* wat2wasm
    (module
      (table (import "test" "table") 2 20 anyfunc)
      (export "tab" (table 0))
    )
    */
    const auto wasm =
        from_hex("0061736d010000000211010474657374057461626c650170010214070701037461620100");

    // importing the table with limits narrower than defined in the module
    std::vector<FuncIdx> table(5);
    auto instance = instantiate(parse(wasm), {}, {ExternalTable{&table, {5, 10}}});

    auto opt_table = find_exported_table(instance, "tab");
    ASSERT_TRUE(opt_table);
    EXPECT_EQ(opt_table->table, &table);
    // table should have the limits it was imported with
    EXPECT_EQ(opt_table->limits.min, 5);
    ASSERT_TRUE(opt_table->limits.max.has_value());
    EXPECT_EQ(*opt_table->limits.max, 10);

    /* wat2wasm
    (module
      (table (import "test" "table") 5 10 anyfunc)
    )
    */
    const auto wasm_reimported_table =
        from_hex("0061736d010000000211010474657374057461626c65017001050a");

    // importing the same table into the module with equal limits, instantiate should succeed
    instantiate(parse(wasm_reimported_table), {}, {*opt_table});
}

TEST(api, find_exported_memory)
{
    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 1 2)
    )
    */
    const auto wasm = from_hex(
        "0061736d01000000010401600000030201000404017000000504010101020606017f0041000b07160401660000"
        "0267310300037461620100036d656d02000a05010300010b");

    auto instance = instantiate(parse(wasm));

    auto opt_memory = find_exported_memory(instance, "mem");
    ASSERT_TRUE(opt_memory);
    EXPECT_EQ(opt_memory->data->size(), PageSize);
    EXPECT_EQ(opt_memory->limits.min, 1);
    ASSERT_TRUE(opt_memory->limits.max.has_value());
    EXPECT_EQ(opt_memory->limits.max, 2);

    EXPECT_FALSE(find_exported_memory(instance, "mem2").has_value());

    /* wat2wasm
    (module
      (memory (import "test" "memory") 1 10)
      (export "mem" (memory 0))
      (func $f (export "f") nop)
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
    )
    */
    const auto wasm_reexported_memory = from_hex(
        "0061736d010000000104016000000211010474657374066d656d6f72790201010a030201000404017000000606"
        "017f0041000b071604036d656d02000166000002673103000374616201000a05010300010b");

    bytes memory(PageSize, 0);
    auto instance_reexported_memory =
        instantiate(parse(wasm_reexported_memory), {}, {}, {ExternalMemory{&memory, {1, 4}}});

    opt_memory = find_exported_memory(instance_reexported_memory, "mem");
    ASSERT_TRUE(opt_memory);
    EXPECT_EQ(opt_memory->data, &memory);
    EXPECT_EQ(opt_memory->limits.min, 1);
    ASSERT_TRUE(opt_memory->limits.max.has_value());
    EXPECT_EQ(opt_memory->limits.max, 4);

    EXPECT_FALSE(find_exported_memory(instance, "memory").has_value());

    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
    )
    */
    const auto wasm_no_memory = from_hex(
        "0061736d01000000010401600000030201000404017000000606017f0041000b07100301660000026731030003"
        "74616201000a05010300010b");

    auto instance_no_memory = instantiate(parse(wasm_no_memory));

    EXPECT_FALSE(find_exported_table(instance_no_memory, "mem").has_value());
}

TEST(api, DISABLED_find_exported_memory_reimport)
{
    /* wat2wasm
    (module
      (memory (import "test" "memory") 1 10)
      (export "mem" (memory 0))
    )
    */
    const auto wasm =
        from_hex("0061736d010000000211010474657374066d656d6f72790201010a070701036d656d0200");

    // importing the memory with limits narrower than defined in the module
    bytes memory(2 * PageSize, 0);
    auto instance = instantiate(parse(wasm), {}, {}, {ExternalMemory{&memory, {2, 5}}});

    auto opt_memory = find_exported_memory(instance, "mem");
    ASSERT_TRUE(opt_memory);
    EXPECT_EQ(opt_memory->data, &memory);
    // table should have the limits it was imported with
    EXPECT_EQ(opt_memory->limits.min, 2);
    ASSERT_TRUE(opt_memory->limits.max.has_value());
    EXPECT_EQ(*opt_memory->limits.max, 5);

    /* wat2wasm
    (module
      (memory (import "test" "memory") 2 5)
    )
    */
    const auto wasm_reimported_memory =
        from_hex("0061736d010000000211010474657374066d656d6f727902010205");

    // importing the same table into the module with equal limits, instantiate should succeed
    instantiate(parse(wasm_reimported_memory), {}, {}, {*opt_memory});
}
