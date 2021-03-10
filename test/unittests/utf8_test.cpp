// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "utf8.hpp"
#include <gtest/gtest.h>
#include <test/utils/hex.hpp>
#include <test/utils/utf8_demo.hpp>

using namespace fizzy;
using namespace fizzy::test;

namespace
{
inline bool utf8_validate(bytes_view input)
{
    return fizzy::utf8_validate(input.begin(), input.end());
}
}  // namespace

TEST(utf8, invalid_first_bytes)
{
    std::vector<bytes> testcases;
    for (uint8_t c = 0x80; c < 0xC2; c++)
        testcases.push_back(bytes(1, c));
    for (uint8_t c = 0xF5; c < 0xFF; c++)
        testcases.push_back(bytes(1, c));
    testcases.push_back("ff"_bytes);
    for (const auto& testcase : testcases)
        EXPECT_FALSE(utf8_validate(testcase));
}

TEST(utf8, validate)
{
    std::vector<std::pair<std::string_view, bool>> testcases = {
        // ASCII
        {"00", true},
        {"7f", true},

        // Missing next byte
        {"80", false},

        // 80..C1
        {"81", false},
        {"c1", false},

        // C2..DF
        {"c2", false},
        {"c280", true},
        {"c2bf", true},
        {"c2c0", false},
        {"dfbf", true},

        // E0
        {"e0", false},
        {"e080", false},
        {"e09f80", false},
        {"e0a0", false},
        {"e0a080", true},
        {"e0a0bf", true},
        {"e0a0c0", false},
        {"e0bfbf", true},

        // E1..EC
        {"e1", false},
        {"e170", false},
        {"e180", false},
        {"e18080", true},
        {"e1807f", false},
        {"e1bfbf", true},

        // ED
        {"ed", false},
        {"ed70", false},
        {"ed80", false},
        {"ed8070", false},
        {"ed8080", true},
        {"ed9fbf", true},
        {"edbfbf", false},
        {"eda080", false},
        {"ed80c0", false},

        // EE..EF
        {"ee", false},
        {"ee70", false},
        {"ee80", false},
        {"ee8070", false},
        {"ee8080", true},
        {"ee80bf", true},
        {"eebfbf", true},
        {"eec080", false},
        {"ee80c0", false},

        // F0
        {"f0", false},
        {"f080", false},
        {"f090", false},
        {"f09070", false},
        {"f0908070", false},
        {"f0908080", true},
        {"f0bfbfbf", true},
        {"f0c0bfbf", false},
        {"f0bfc0bf", false},
        {"f0bfbfc0", false},

        // F1..F3
        {"f1", false},
        {"f170", false},
        {"f180", false},
        {"f18070", false},
        {"f1808070", false},
        {"f1808080", true},
        {"f1bfbfbf", true},
        {"f1c0bfbf", false},
        {"f1bfc0bf", false},
        {"f1bfbfc0", false},

        // F4
        {"f4", false},
        {"f470", false},
        {"f480", false},
        {"f48070", false},
        {"f4808070", false},
        {"f4808080", true},
        {"f48fbfbf", true},
        {"f490bfbf", false},
        {"f48fc0bf", false},
        {"f48fbfc0", false},

        // Multi-character example
        {"616263c2bfe0a080ecbabaed9fbfee8181efaa81f09081a0f1a0a081f4819f85", true},
    };
    for (const auto& testcase : testcases)
    {
        const auto input = fizzy::test::from_hex(testcase.first);
        EXPECT_EQ(utf8_validate(input), testcase.second);
    }
}

TEST(utf8, missing_second_byte)
{
    constexpr uint8_t first_bytes[]{0xDF, 0xE0, 0xEC, 0xED, 0xEF, 0xF0, 0xF3, 0xF4};
    for (const auto b : first_bytes)
    {
        const uint8_t input[]{b};
        EXPECT_FALSE(utf8_validate({input, std::size(input)}));
    }
}

TEST(utf8, missing_third_byte)
{
    constexpr uint8_t first_bytes[]{0xE0, 0xEC, 0xED, 0xEF, 0xF0, 0xF3, 0xF4};
    for (const auto b : first_bytes)
    {
        const uint8_t input[]{b, 0xA0};
        EXPECT_FALSE(utf8_validate({input, std::size(input)}));
    }
}

TEST(utf8, missing_forth_byte)
{
    constexpr uint8_t first_bytes[]{0xF0, 0xF3, 0xF4};
    for (const auto b : first_bytes)
    {
        const uint8_t input[]{b, 0xA0, 0xA0};
        EXPECT_FALSE(utf8_validate({input, std::size(input)}));
    }
}

TEST(utf8, validate_utf8_demo)
{
    const bytes_view utf8_demo_bytes{
        reinterpret_cast<const uint8_t*>(utf8_demo.data()), utf8_demo.size()};
    EXPECT_TRUE(utf8_validate(utf8_demo_bytes));
}
