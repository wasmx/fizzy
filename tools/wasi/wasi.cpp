// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include <uvwasi.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
bool run(int argc, const char** argv)
{
    (void)argc;
    (void)argv;
    return false;
}
}  // namespace

int main(int argc, const char** argv)
{
    try
    {
        if (argc < 2)
        {
            std::cerr << "Missing executable argument\n";
            return -1;
        }

        // Remove fizzy-wasi from the argv, but keep "argv[1]"
        const bool res = run(argc - 1, argv + 1);
        return res ? 0 : 1;
    }
    catch (const std::exception& ex)
    {
        std::cerr << "Exception: " << ex.what() << "\n";
        return -2;
    }
}
