// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "utf8.hpp"
#include <cassert>

/*
 * The Unicode Standard, Version 6.0
 * (https://www.unicode.org/versions/Unicode6.0.0/ch03.pdf)
 *
 * Page 94, Table 3-7. Well-Formed UTF-8 Byte Sequences
 *
 * +--------------------+------------+-------------+------------+-------------+
 * | Code Points        | First Byte | Second Byte | Third Byte | Fourth Byte |
 * +--------------------+------------+-------------+------------+-------------+
 * | U+0000..U+007F     | 00..7F     |             |            |             |
 * +--------------------+------------+-------------+------------+-------------+
 * | U+0080..U+07FF     | C2..DF     | 80..BF      |            |             |
 * +--------------------+------------+-------------+------------+-------------+
 * | U+0800..U+0FFF     | E0         | A0..BF      | 80..BF     |             |
 * +--------------------+------------+-------------+------------+-------------+
 * | U+1000..U+CFFF     | E1..EC     | 80..BF      | 80..BF     |             |
 * +--------------------+------------+-------------+------------+-------------+
 * | U+D000..U+D7FF     | ED         | 80..9F      | 80..BF     |             |
 * +--------------------+------------+-------------+------------+-------------+
 * | U+E000..U+FFFF     | EE..EF     | 80..BF      | 80..BF     |             |
 * +--------------------+------------+-------------+------------+-------------+
 * | U+10000..U+3FFFF   | F0         | 90..BF      | 80..BF     | 80..BF      |
 * +--------------------+------------+-------------+------------+-------------+
 * | U+40000..U+FFFFF   | F1..F3     | 80..BF      | 80..BF     | 80..BF      |
 * +--------------------+------------+-------------+------------+-------------+
 * | U+100000..U+10FFFF | F4         | 80..8F      | 80..BF     | 80..BF      |
 * +--------------------+------------+-------------+------------+-------------+
 */

namespace
{
// Ranges for 2nd byte
enum class Rule
{
    Range80BF,  // 80..BF
    RangeA0BF,  // A0..BF
    Range809F,  // 80..9F
    Range90BF,  // 90..BF
    Range808F,  // 80..8F
};
}  // namespace

namespace fizzy
{
bool utf8_validate(const uint8_t* pos, const uint8_t* end) noexcept
{
    while (pos < end)
    {
        const uint8_t byte1 = *pos++;
        if (byte1 <= 0x7F)
            continue;  // Shortcut for valid ASCII (also valid UTF-8)

        if (byte1 < 0xC2)
            return false;

        // Discover requirements.
        int required_bytes;
        Rule byte2_rule;
        if (byte1 <= 0xDF)
        {
            required_bytes = 2;
            byte2_rule = Rule::Range80BF;
        }
        else if (byte1 == 0xE0)
        {
            required_bytes = 3;
            byte2_rule = Rule::RangeA0BF;
        }
        else if (byte1 <= 0xEC)
        {  // NOLINT(bugprone-branch-clone)
            required_bytes = 3;
            byte2_rule = Rule::Range80BF;
        }
        else if (byte1 == 0xED)
        {
            required_bytes = 3;
            byte2_rule = Rule::Range809F;
        }
        else if (byte1 <= 0xEF)
        {
            required_bytes = 3;
            byte2_rule = Rule::Range80BF;
        }
        else if (byte1 == 0xF0)
        {
            required_bytes = 4;
            byte2_rule = Rule::Range90BF;
        }
        else if (byte1 <= 0xF3)
        {
            required_bytes = 4;
            byte2_rule = Rule::Range80BF;
        }
        else if (byte1 == 0xF4)
        {
            required_bytes = 4;
            byte2_rule = Rule::Range808F;
        }
        else
            return false;


        // At this point need to read at least one more byte.
        assert(required_bytes > 1);
        if ((end - pos) < (required_bytes - 1))
            return false;


        // Byte2 may have exceptional encodings
        const uint8_t byte2 = *pos++;
        switch (byte2_rule)
        {
        case Rule::Range80BF:
            if (byte2 < 0x80 || byte2 > 0xBF)
                return false;
            break;
        case Rule::RangeA0BF:
            if (byte2 < 0xA0 || byte2 > 0xBF)
                return false;
            break;
        case Rule::Range809F:
            if (byte2 < 0x80 || byte2 > 0x9F)
                return false;
            break;
        case Rule::Range90BF:
            if (byte2 < 0x90 || byte2 > 0xBF)
                return false;
            break;
        case Rule::Range808F:
            if (byte2 < 0x80 || byte2 > 0x8F)
                return false;
            break;
            // NOTE: no default case required since it is enum class
        }

        // Byte3 always has regular encoding
        if (required_bytes > 2)
        {
            const uint8_t byte3 = *pos++;
            if (byte3 < 0x80 || byte3 > 0xBF)
                return false;
        }

        // Byte4 always has regular encoding
        if (required_bytes > 3)
        {
            const uint8_t byte4 = *pos++;
            if (byte4 < 0x80 || byte4 > 0xBF)
                return false;
        }
    }

    assert(pos == end);

    return true;
}

}  // namespace fizzy
