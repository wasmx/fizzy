// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "parser.hpp"
#include <fizzy/fizzy.h>

extern "C" {
bool fizzy_validate(const uint8_t* wasm_binary, size_t wasm_binary_size)
{
    try
    {
        fizzy::parse({wasm_binary, wasm_binary_size});
        return true;
    }
    catch (...)
    {
        return false;
    }
}
}
