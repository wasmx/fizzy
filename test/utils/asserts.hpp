// Fizzy: A fast WebAssembly interpreter
// Copyright 2019-2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "execute.hpp"
#include "value.hpp"
#include <fizzy/fizzy.h>
#include <gmock/gmock.h>
#include <test/utils/floating_point_utils.hpp>
#include <test/utils/typed_value.hpp>
#include <iosfwd>
#include <type_traits>

MATCHER(Traps, "")  // NOLINT(readability-redundant-string-init)
{
    return arg.trapped;
}

MATCHER(Result, "empty result")
{
    return !arg.trapped && !arg.has_value;
}

MATCHER_P(Result, value, "")  // NOLINT(readability-redundant-string-init)
{
    using namespace fizzy;

    // Require the arg to be TypedExecutionResult.
    // This can be a static_assert, but just returning false and failing a test provides better
    // location of the error.
    using result_type = std::remove_cv_t<std::remove_reference_t<arg_type>>;
    if constexpr (!std::is_same_v<result_type, test::TypedExecutionResult>)
    {
        if (result_listener->IsInterested())
            *result_listener << "TypedExecutionResult expected";
        return false;
    }
    else
    {
        if (arg.trapped || !arg.has_value)
            return false;

        if constexpr (std::is_same_v<value_type, float>)
        {
            return arg.type == ValType::f32 && arg.value.f32 == test::FP{value};
        }

        if constexpr (std::is_same_v<value_type, double>)
        {
            return arg.type == ValType::f64 && arg.value.f64 == test::FP{value};
        }

        if constexpr (std::is_integral_v<value_type> && sizeof(value_type) == sizeof(uint64_t))
        {
            return arg.type == ValType::i64 && arg.value.i64 == static_cast<uint64_t>(value);
        }

        if constexpr (std::is_integral_v<value_type> && sizeof(value_type) == sizeof(uint32_t))
        {
            if (arg.type == ValType::i32)
            {
                return arg.value.i64 == static_cast<uint32_t>(value);
            }
            else if (arg.type == ValType::i64 && value >= 0)
            {
                // Here allow convenient zero-extension of the expected value u32 -> u64.
                return arg.value.i64 == static_cast<uint32_t>(value);
            }
            return false;
        }

        if (result_listener->IsInterested())
            *result_listener << "expected value has non-wasm type";
        return false;
    }
}

MATCHER(CTraps, "")  // NOLINT(readability-redundant-string-init)
{
    return arg.trapped;
}

MATCHER(CResult, "empty result")
{
    return !arg.trapped && !arg.has_value;
}

MATCHER_P(CResult, value, "")  // NOLINT(readability-redundant-string-init)
{
    if (arg.trapped || !arg.has_value)
        return false;

    if constexpr (std::is_floating_point_v<value_type>)
    {
        if constexpr (std::is_same_v<float, value_type>)
            return arg.value.f32 == fizzy::test::FP{value};
        else
        {
            static_assert(std::is_same_v<double, value_type>);
            return arg.value.f64 == fizzy::test::FP{value};
        }
    }
    else  // always check 64 bit of result for all integers, including 32-bit results
        return arg.value.i64 == static_cast<std::make_unsigned_t<value_type>>(value);
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

std::ostream& operator<<(std::ostream& os, FizzyExecutionResult);

namespace fizzy
{
/// Equal operator for Instr and uint8_t. Convenient for unit tests.
inline constexpr bool operator==(uint8_t a, Instr b) noexcept
{
    return a == static_cast<uint8_t>(b);
}

std::ostream& operator<<(std::ostream& os, ExecutionResult);
}  // namespace fizzy

namespace fizzy::test
{
inline uint32_t as_uint32(fizzy::Value value)
{
    EXPECT_EQ(value.i64 & 0xffffffff00000000, 0);
    return static_cast<uint32_t>(value.i64);
}

std::ostream& operator<<(std::ostream& os, const TypedExecutionResult&);
}  // namespace fizzy::test
