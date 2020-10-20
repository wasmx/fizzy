// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "asserts.hpp"

#ifdef GCOV
extern "C" void __gcov_flush();
#endif

namespace fizzy
{
bool unreachable() noexcept
{
#ifdef GCOV
    __gcov_flush();
#endif
    return false;  // LCOV_EXCL_LINE
}
}  // namespace fizzy
