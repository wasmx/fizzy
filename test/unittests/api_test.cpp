#include "execute.hpp"
#include <gtest/gtest.h>

using namespace fizzy;

TEST(api, find_exported_function)
{
    Module module;
    module.exportsec.emplace_back(Export{"foo1", ExportType::Function, 0});
    module.exportsec.emplace_back(Export{"foo2", ExportType::Function, 1});
    module.exportsec.emplace_back(Export{"foo3", ExportType::Function, 2});
    module.exportsec.emplace_back(Export{"foo4", ExportType::Function, 42});
    module.exportsec.emplace_back(Export{"mem", ExportType::Memory, 0});
    module.exportsec.emplace_back(Export{"glob", ExportType::Global, 0});
    module.exportsec.emplace_back(Export{"table", ExportType::Table, 0});

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
