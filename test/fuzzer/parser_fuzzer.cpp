// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <iomanip>
#include <iostream>

#include <src/binary-reader-ir.h>
#include <src/binary-reader.h>
#include <src/ir.h>
#include <src/validator.h>

namespace
{
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

wabt::Errors wabt_errors;

bool wabt_parse(const uint8_t* data, size_t data_size) noexcept
{
    using namespace wabt;

    ReadBinaryOptions read_options;
    read_options.features.enable_mutable_globals();
    Module module;

    wabt_errors.clear();

    {
        const auto result =
            ReadBinaryIr("fuzzing", data, data_size, read_options, &wabt_errors, &module);

        if (Failed(result))
            return false;
    }

    {
        const auto result =
            ValidateModule(&module, &wabt_errors, ValidateOptions{read_options.features});
        if (Failed(result))
            return false;
    }

    return wabt_errors.empty();
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t data_size) noexcept
{
    const auto expected = wabt_parse(data, data_size);

    try
    {
        fizzy::parse({data, data_size});
        ++stats.valid;
        if (!expected)
        {
            for (const auto& err : wabt_errors)
            {
                std::cerr << "MISSED ERROR: " << err.message << "\n";
            }
            __builtin_trap();
        }
    }
    catch (const fizzy::parser_error& err)
    {
        ++stats.malformed;
        if (expected)
        {
            std::cerr << "\nMALFORMED: " << err.what() << "\n\n";
            __builtin_trap();
        }
    }
    catch (const fizzy::validation_error& err)
    {
        ++stats.invalid;
        if (expected)
        {
            std::cerr << "\nINVALID: " << err.what() << "\n\n";
            __builtin_trap();
        }
    }

    return 0;
}
