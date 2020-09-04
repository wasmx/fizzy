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

    ~parser_error() noexcept override;
};

struct validation_error : public std::runtime_error
{
    using runtime_error::runtime_error;

    ~validation_error() noexcept override;
};

struct instantiate_error : public std::runtime_error
{
    using runtime_error::runtime_error;

    ~instantiate_error() noexcept override;
};

}  // namespace fizzy
