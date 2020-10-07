// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace fizzy
{
// The page size as defined by the WebAssembly 1.0 specification.
constexpr uint32_t PageSize = 65536;

// The maximum memory page limit as defined by the specification.
// It is only possible to address 4 GB (32-bit) of memory.
constexpr uint32_t MemoryPagesValidationLimit = (4 * 1024 * 1024 * 1024ULL) / PageSize;
static_assert(MemoryPagesValidationLimit == 65536);

// The default hard limit of the memory size (256MB) as number of pages.
constexpr uint32_t DefaultMemoryPagesLimit = (256 * 1024 * 1024ULL) / PageSize;

// Call depth limit is set to default limit in wabt.
// https://github.com/WebAssembly/wabt/blob/ae2140ddc6969ef53599fe2fab81818de65db875/src/interp/interp.h#L1007
// TODO: review this
constexpr int CallStackLimit = 2048;
}  // namespace fizzy
