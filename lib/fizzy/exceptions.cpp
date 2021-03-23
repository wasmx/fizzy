// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "exceptions.hpp"

namespace fizzy
{
exception::~exception() noexcept = default;
parser_error::~parser_error() noexcept = default;
validation_error::~validation_error() noexcept = default;
instantiate_error::~instantiate_error() noexcept = default;
}  // namespace fizzy
