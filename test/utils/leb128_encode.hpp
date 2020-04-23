// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "bytes.hpp"

namespace fizzy::test
{
/// Encodes the value as unsigned LEB128.
fizzy::bytes leb128u_encode(uint64_t value) noexcept;
}  // namespace fizzy::test
