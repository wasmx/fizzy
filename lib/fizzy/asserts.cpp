// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "asserts.hpp"

#ifdef GCOV
/// Explicitly dump coverage counters.
/// https://gcc.gnu.org/onlinedocs/gcc/Gcov-and-Optimization.html
extern "C" void __gcov_dump();
#endif

namespace fizzy
{
bool unreachable() noexcept
{
#ifdef GCOV
    __gcov_dump();
#endif
    return false;  // LCOV_EXCL_LINE
}
}  // namespace fizzy
