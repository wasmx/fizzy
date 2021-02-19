// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "wasi.hpp"
#include <iostream>

int main(int argc, const char** argv)
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Missing executable argument\n";
            return -1;
        }

        // Drop "fizzy-wasi" from the argv, but keep the wasm file name in argv[1].
        const bool res = fizzy::wasi::load_and_run(argc - 1, argv + 1, std::cerr);
        return res ? 0 : 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return -2;
    }
}
