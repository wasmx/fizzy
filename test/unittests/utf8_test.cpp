#include "utf8.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;

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
    std::vector<std::pair<bytes, bool>> testcases = {
        // ASCII
        {"00"_bytes, true},
        {"7f"_bytes, true},

        // Missing next byte
        {"80"_bytes, false},

        // 80..C1
        {"81"_bytes, false},
        {"c1"_bytes, false},

        // C2..DF
        {"c2"_bytes, false},
        {"c280"_bytes, true},
        {"c2bf"_bytes, true},
        {"c2c0"_bytes, false},
        {"dfbf"_bytes, true},

        // E0
        {"e0"_bytes, false},
        {"e080"_bytes, false},
        {"e09f80"_bytes, false},
        {"e0a0"_bytes, false},
        {"e0a080"_bytes, true},
        {"e0a0bf"_bytes, true},
        {"e0a0c0"_bytes, false},
        {"e0bfbf"_bytes, true},

        // E1..EC
        {"e1"_bytes, false},
        {"e170"_bytes, false},
        {"e180"_bytes, false},
        {"e18080"_bytes, true},
        {"e1807f"_bytes, false},
        {"e1bfbf"_bytes, true},

        // ED
        {"ed"_bytes, false},
        {"ed70"_bytes, false},
        {"ed80"_bytes, false},
        {"ed8070"_bytes, false},
        {"ed8080"_bytes, true},
        {"ed9fbf"_bytes, true},
        {"edbfbf"_bytes, false},
        {"eda080"_bytes, false},
        {"ed80c0"_bytes, false},

        // EE..EF
        {"ee"_bytes, false},
        {"ee70"_bytes, false},
        {"ee80"_bytes, false},
        {"ee8070"_bytes, false},
        {"ee8080"_bytes, true},
        {"ee80bf"_bytes, true},
        {"eebfbf"_bytes, true},
        {"eec080"_bytes, false},
        {"ee80c0"_bytes, false},

        // F0
        {"f0"_bytes, false},
        {"f080"_bytes, false},
        {"f090"_bytes, false},
        {"f09070"_bytes, false},
        {"f0908070"_bytes, false},
        {"f0908080"_bytes, true},
        {"f0bfbfbf"_bytes, true},
        {"f0c0bfbf"_bytes, false},
        {"f0bfc0bf"_bytes, false},
        {"f0bfbfc0"_bytes, false},

        // F1..F3
        {"f1"_bytes, false},
        {"f170"_bytes, false},
        {"f180"_bytes, false},
        {"f18070"_bytes, false},
        {"f1808070"_bytes, false},
        {"f1808080"_bytes, true},
        {"f1bfbfbf"_bytes, true},
        {"f1c0bfbf"_bytes, false},
        {"f1bfc0bf"_bytes, false},
        {"f1bfbfc0"_bytes, false},

        // F4
        {"f4"_bytes, false},
        {"f470"_bytes, false},
        {"f480"_bytes, false},
        {"f48070"_bytes, false},
        {"f4808070"_bytes, false},
        {"f4808080"_bytes, true},
        {"f48fbfbf"_bytes, true},
        {"f490bfbf"_bytes, false},
        {"f48fc0bf"_bytes, false},
        {"f48fbfc0"_bytes, false},

        // Multi-character example
        {"616263c2bfe0a080ecbabaed9fbfee8181efaa81f09081a0f1a0a081f4819f85"_bytes, true},
    };
    for (const auto& testcase : testcases)
        EXPECT_EQ(utf8_validate(testcase.first), testcase.second);
}
