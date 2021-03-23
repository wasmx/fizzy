// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <fizzy/fizzy_errors.h>
#include <stdexcept>
#include <string>

namespace fizzy
{
struct exception : std::runtime_error
{
    FizzyErrorCode code = FIZZY_ERROR_OTHER;

    exception(FizzyErrorCode _code, const char* message) noexcept
      : std::runtime_error(message), code(_code)
    {}
    exception(FizzyErrorCode _code, const std::string& message) noexcept
      : std::runtime_error(message.c_str()), code(_code)
    {}
    // TODO sets default error code, to be removed
    explicit exception(const char* message) noexcept : std::runtime_error(message) {}
    explicit exception(const std::string& message) noexcept : std::runtime_error(message) {}
    exception(const exception& other) noexcept : std::runtime_error(other), code(other.code) {}
    ~exception() noexcept override;
};

struct parser_error : exception
{
    using exception::exception;

    ~parser_error() noexcept override;
};

struct validation_error : exception
{
    using exception::exception;

    ~validation_error() noexcept override;
};

struct instantiate_error : exception
{
    using exception::exception;

    ~instantiate_error() noexcept override;
};

}  // namespace fizzy
