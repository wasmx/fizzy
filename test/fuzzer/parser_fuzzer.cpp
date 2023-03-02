// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t data_size) noexcept
{
    try
    {
        fizzy::parse({data, data_size});
    }
    catch (const fizzy::parser_error&)
    {}
    catch (const fizzy::validation_error&)
    {}
    return 0;
}
