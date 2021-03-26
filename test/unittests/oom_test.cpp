// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

/// @file
/// Special unit test suite testing out-of-memory situation.
/// Tests and checks are powered by OS-specific configuration and interaction if available.

#include "execute.hpp"
#include "parser.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>

#ifdef __linux__
#include <sys/resource.h>
#endif

#ifndef __has_feature
#define __has_feature(X) 0
#endif

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#define ASAN 1
#else
#define ASAN 0
#endif

namespace
{
/// The OS-specific memory limit in bytes.
/// Exact meaning depends on OS.
/// - For Linux this is virtual address space limit of the process (i.e. fizzy-unittests).
///   The value must be multiple of the OS page size (4096) and bigger than the current usage
///   at the point of setting the limit (otherwise it returns success, but has no effect).
constexpr uint32_t OSMemoryLimitBytes = 2 * 1024 * 1024;

/// Tries to set the OS memory limit. Returns true if the limit has been set.
bool try_set_memory_limit(size_t size) noexcept;

/// Lifts the previously set memory limit. Returns false in case of unexpected error.
bool restore_memory_limit() noexcept;

#if !ASAN && defined(__linux__) && (defined(__x86_64__) || defined(__i386__))

rlimit orig_limit;

bool try_set_memory_limit(size_t size) noexcept
{
    [[maybe_unused]] int err = getrlimit(RLIMIT_AS, &orig_limit);  // Record original limit.
    assert(err == 0);
    auto limit = orig_limit;
    limit.rlim_cur = size;  // Set the soft limit, leaving the hard limit unchanged.
    err = setrlimit(RLIMIT_AS, &limit);
    assert(err == 0);
    return true;
}

bool restore_memory_limit() noexcept
{
    return setrlimit(RLIMIT_AS, &orig_limit) == 0;
}

#else

bool try_set_memory_limit([[maybe_unused]] size_t size) noexcept
{
    return false;
}

bool restore_memory_limit() noexcept
{
    return true;
}

#endif
}  // namespace

using namespace fizzy;
using namespace fizzy::test;

TEST(oom, execute_memory_grow)
{
    /* wat2wasm
    (memory 0)
    (func (param i32) (result i32)
      local.get 0
      memory.grow
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017f0302010005030100000a08010600200040000b");

    auto instance = instantiate(parse(wasm), {}, {}, {}, {}, MaxMemoryPagesLimit);

    const auto is_limited = try_set_memory_limit(OSMemoryLimitBytes);
    const auto expected_result = is_limited ? uint32_t(-1) : uint32_t{0};
    constexpr uint32_t memory_grow_page_count = OSMemoryLimitBytes / PageSize;
    EXPECT_LE(memory_grow_page_count, MaxMemoryPagesLimit);
    EXPECT_THAT(execute(*instance, 0, {memory_grow_page_count}), Result(expected_result));
    ASSERT_TRUE(restore_memory_limit());
}
