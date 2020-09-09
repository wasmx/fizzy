// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <cstdint>
#include <utility>

std::pair<uint64_t, const uint8_t*> decodeULEB128(const uint8_t* p, const uint8_t* end);
