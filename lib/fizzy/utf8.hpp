// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>

namespace fizzy
{
bool utf8_validate(const uint8_t* pos, const uint8_t* end) noexcept;
}  // namespace fizzy
