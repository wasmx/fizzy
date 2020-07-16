// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "execute.hpp"
#include <gmock/gmock.h>
#include <iosfwd>

MATCHER(Traps, "")  // NOLINT(readability-redundant-string-init)
{
    return arg.trapped || arg.errored;
}

MATCHER(Result, "empty result")
{
    return !arg.trapped && !arg.errored && !arg.has_value;
}

MATCHER_P(Result, value, "")  // NOLINT(readability-redundant-string-init)
{
    return !arg.trapped && !arg.errored && arg.has_value && arg.value == uint64_t(value);
}

#define EXPECT_THROW_MESSAGE(stmt, ex_type, expected)                                        \
    try                                                                                      \
    {                                                                                        \
        stmt;                                                                                \
        ADD_FAILURE() << "Exception of type " #ex_type " is expected, but none was thrown."; \
    }                                                                                        \
    catch (const ex_type& exception)                                                         \
    {                                                                                        \
        EXPECT_STREQ(exception.what(), expected);                                            \
    }                                                                                        \
    catch (const std::exception& exception)                                                  \
    {                                                                                        \
        ADD_FAILURE() << "Unexpected exception type thrown: " << exception.what() << ".";    \
    }                                                                                        \
    catch (...)                                                                              \
    {                                                                                        \
        ADD_FAILURE() << "Unexpected exception type thrown.";                                \
    }                                                                                        \
    (void)0

namespace fizzy
{
std::ostream& operator<<(std::ostream& os, ExecutionResult);
}
