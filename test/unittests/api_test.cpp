#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
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
      (global (export "g1") i32 (i32.const 0))
      (global (export "g2") i32 (i32.const 1))
      (global (export "g3") i32 (i32.const 2))
      (global (export "g4") i32 (i32.const 3))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 0)
    )
     */
    const auto wasm = from_hex(
        "0061736d010000000104016000000302010004040170000005030100000615047f0041000b7f0041010b7f0041"
        "020b7f0041030b072507016600000267310300026732030102673303020267340303037461620100036d656d02"
        "000a05010300010b");

    const auto module = parse(wasm);

    auto optionalIdx = find_exported_global(module, "g1");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 0);

    optionalIdx = find_exported_global(module, "g2");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 1);

    optionalIdx = find_exported_global(module, "g3");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 2);

    optionalIdx = find_exported_global(module, "g4");
    ASSERT_TRUE(optionalIdx);
    EXPECT_EQ(*optionalIdx, 3);

    EXPECT_FALSE(find_exported_global(module, "f"));
    EXPECT_FALSE(find_exported_global(module, "tab"));
    EXPECT_FALSE(find_exported_global(module, "mem"));
}

TEST(api, find_exported_table_memory)
{
    /* wat2wasm
    (module
      (func $f (export "f") nop)
      (global (export "g1") i32 (i32.const 0))
      (table (export "tab") 0 anyfunc)
      (memory (export "mem") 0)
    )
     */
    const auto wasm = from_hex(
        "0061736d010000000104016000000302010004040170000005030100000606017f0041000b0716040166000002"
        "67310300037461620100036d656d02000a05010300010b");

    const auto module = parse(wasm);

    auto optionalName = find_exported_table_name(module);
    ASSERT_TRUE(optionalName);
    EXPECT_EQ(*optionalName, "tab");

    optionalName = find_exported_memory_name(module);
    ASSERT_TRUE(optionalName);
    EXPECT_EQ(*optionalName, "mem");

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
    const auto module_no_table = parse(wasm_no_table);

    EXPECT_FALSE(find_exported_table_name(module_no_table));

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
    const auto module_no_memory = parse(wasm_no_memory);

    EXPECT_FALSE(find_exported_memory_name(module_no_memory));
}
