#include "execute.hpp"
#include <gtest/gtest.h>

TEST(api, find_exported_function)
{
    fizzy::Module module;
    module.exportsec.emplace_back(fizzy::Export{"foo1", fizzy::ExportType::Function, 0});
    module.exportsec.emplace_back(fizzy::Export{"foo2", fizzy::ExportType::Function, 1});
    module.exportsec.emplace_back(fizzy::Export{"foo3", fizzy::ExportType::Function, 2});
    module.exportsec.emplace_back(fizzy::Export{"foo4", fizzy::ExportType::Function, 42});
    module.exportsec.emplace_back(fizzy::Export{"mem", fizzy::ExportType::Memory, 0});
    module.exportsec.emplace_back(fizzy::Export{"glob", fizzy::ExportType::Global, 0});
    module.exportsec.emplace_back(fizzy::Export{"table", fizzy::ExportType::Table, 0});

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
