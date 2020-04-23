// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <string>

namespace fizzy
{
using bytes = std::basic_string<uint8_t>;
using bytes_view = std::basic_string_view<uint8_t>;

}  // namespace fizzy
