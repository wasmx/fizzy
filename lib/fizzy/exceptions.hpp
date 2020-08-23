// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <stdexcept>

namespace fizzy
{
struct parser_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

struct validation_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

struct instantiate_error : public std::runtime_error
{
    using runtime_error::runtime_error;
};

}  // namespace fizzy
