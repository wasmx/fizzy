// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <iomanip>
#include <iostream>

struct Stats
{
    int64_t malformed = 0;
    int64_t invalid = 0;
    int64_t valid = 0;

    ~Stats()
    {
        const auto all = malformed + invalid + valid;
        if (all == 0)
            return;
        std::clog << "WASM STATS" << std::setprecision(3) << "\n  all:       " << all
                  << "\n  malformed: " << malformed << " " << ((malformed * 100) / all) << "%"
                  << "\n  invalid:   " << invalid << " " << ((invalid * 100) / all) << "%"
                  << "\n  valid:     " << valid << " " << ((valid * 100) / all) << "%"
                  << "\n";
    }
};

Stats stats;

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t data_size) noexcept
{
    try
    {
        fizzy::parse({data, data_size});
        ++stats.valid;
    }
    catch (const fizzy::parser_error&)
    {
        ++stats.malformed;
    }
    catch (const fizzy::validation_error&)
    {
        ++stats.invalid;
    }
    return 0;
}
