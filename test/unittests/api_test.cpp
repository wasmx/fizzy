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
