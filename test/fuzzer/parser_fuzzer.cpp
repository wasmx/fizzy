// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <cstdlib>
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

void handle_unexpected_errors() noexcept
{
    static const bool ignore_errors = [] {
        const auto options = std::getenv("OPTIONS");
        if (!options)
            return false;
        return std::string{options}.find("ignore_errors") != std::string::npos;
    }();
    if (!ignore_errors)
        __builtin_unreachable();
}

//constexpr auto wabt_ignored_errors = {
//    "unable to read u32 leb128: version",
//    "invalid linking metadata version:",
//};

wabt::Errors wabt_errors;

bool wabt_parse(const uint8_t* data, size_t data_size) noexcept
{
    using namespace wabt;

    ReadBinaryOptions read_options;
    read_options.features.enable_mutable_globals();
    read_options.features.disable_exceptions();
    read_options.features.disable_annotations();
    read_options.features.disable_bulk_memory();
    read_options.features.disable_gc();
    read_options.features.disable_memory64();
    read_options.features.disable_multi_value();
    read_options.features.disable_reference_types();
    read_options.features.disable_sat_float_to_int();
    read_options.features.disable_sign_extension();
    read_options.features.disable_simd();
    read_options.features.disable_tail_call();
    read_options.features.disable_threads();
    read_options.fail_on_custom_section_error = false;
    read_options.stop_on_first_error = true;
    read_options.read_debug_names = false;
    Module module;

    wabt_errors.clear();

    {
        const auto result =
            ReadBinaryIr("fuzzing", data, data_size, read_options, &wabt_errors, &module);

        if (Failed(result))
            return false;

        wabt_errors.clear();  // Clear errors (probably) from custom sections.
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

extern "C" {

size_t LLVMFuzzerMutate(uint8_t* data, size_t size, size_t max_size) noexcept;

size_t LLVMFuzzerCustomMutator(
    uint8_t* data, size_t size, size_t max_size, [[maybe_unused]] unsigned int seed) noexcept
{
    static constexpr uint8_t wasm_prefix[]{0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00};
    static constexpr auto wasm_prefix_size = sizeof(wasm_prefix);

    // For inputs shorter than wasm prefix just mutate it.
    if (size <= wasm_prefix_size)
        return LLVMFuzzerMutate(data, size, max_size);

    // For other, leave prefix unchanged. It is likely to be valid and we don't want to waste time
    // on mutating the prefix.
    const auto new_size_without_prefix = LLVMFuzzerMutate(
        data + wasm_prefix_size, size - wasm_prefix_size, max_size - wasm_prefix_size);
    return new_size_without_prefix + wasm_prefix_size;
}

int LLVMFuzzerTestOneInput(const uint8_t* data, size_t data_size) noexcept
{
    const auto expected = wabt_parse(data, data_size);

    try
    {
        fizzy::parse({data, data_size});
        ++stats.valid;
        if (!expected)
        {
            bool has_errors = false;
            for (const auto& err : wabt_errors)
            {
//                bool ignored = false;
//
//                for (const auto& m : wabt_ignored_errors)
//                {
//                    if (err.message.find(m) != std::string::npos)
//                        ignored = true;
//                }
//                if (ignored)
//                    continue;

                std::cerr << "  MISSED ERROR: " << err.message << "\n";
                has_errors = true;
            }

            if (has_errors)
                handle_unexpected_errors();
        }
    }
    catch (const fizzy::parser_error& err)
    {
        ++stats.malformed;
        if (expected)
        {
            std::cerr << "  MALFORMED: " << err.what() << "\n";
            handle_unexpected_errors();
        }
    }
    catch (const fizzy::validation_error& err)
    {
        ++stats.invalid;
        if (expected)
        {
            std::cerr << "  INVALID: " << err.what() << "\n";
            handle_unexpected_errors();
        }
    }

    return 0;
}
}
