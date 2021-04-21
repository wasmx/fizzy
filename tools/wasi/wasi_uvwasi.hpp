// Fizzy: A fast WebAssembly interpreter
// Copyright 2021 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "wasi.hpp"
#include <memory>

namespace fizzy::wasi
{
std::unique_ptr<WASI> create_uvwasi();
}  // namespace fizzy::wasi
