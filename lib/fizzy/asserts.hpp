// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <cassert>

#ifndef __has_builtin
#define __has_builtin(name) 0
#endif

#ifndef __has_feature
#define __has_feature(name) 0
#endif


namespace fizzy
{
/// A helper for assert(unreachable()). Always returns false. Also flushes coverage data.
bool unreachable() noexcept;
}  // namespace fizzy

#ifndef NDEBUG
#define FIZZY_UNREACHABLE() assert(fizzy::unreachable())
#elif __has_builtin(__builtin_unreachable)
#define FIZZY_UNREACHABLE() __builtin_unreachable()
#else
#define FIZZY_UNREACHABLE() (void)0
#endif
