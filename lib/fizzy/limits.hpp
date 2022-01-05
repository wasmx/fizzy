// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace fizzy
{
/// The page size as defined by the WebAssembly 1.0 specification.
constexpr uint32_t PageSize = 65536;

/// Convert memory size in pages to size in bytes.
inline constexpr uint64_t memory_pages_to_bytes(uint32_t pages) noexcept
{
    return uint64_t{pages} * PageSize;
}

/// The maximum memory page limit as defined by the specification.
/// It is only possible to address 4 GB (32-bit) of memory.
/// For 32-bit build environment only max size_t value can be allocated (4 GB - 1 byte).
constexpr auto MaxMemoryBytesLimit =
    std::min<uint64_t>(4 * 1024 * 1024 * 1024ULL, std::numeric_limits<size_t>::max());
constexpr uint32_t MaxMemoryPagesLimit = MaxMemoryBytesLimit / PageSize;
static_assert((sizeof(size_t) > sizeof(uint32_t) && MaxMemoryPagesLimit == 65536) ||
              MaxMemoryPagesLimit == 65535);
static_assert(memory_pages_to_bytes(MaxMemoryPagesLimit) <= std::numeric_limits<size_t>::max());

/// The default hard limit of the memory size (256MB) as number of pages.
constexpr uint32_t DefaultMemoryPagesLimit = (256 * 1024 * 1024ULL) / PageSize;
static_assert(DefaultMemoryPagesLimit == 4096);

/// The limit of the size of the call stack, i.e. how many calls are allowed to be stacked up
/// in a single execution thread. Allowed values for call depth levels are [0, CallStackLimit-1].
/// The current value is the same as the default limit in WABT:
/// https://github.com/WebAssembly/wabt/blob/1.0.20/src/interp/interp.h#L1027
constexpr int CallStackLimit = 2048;
}  // namespace fizzy
