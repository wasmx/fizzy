#pragma once

#include <gtest/gtest.h>

#define EXPECT_RESULT(result, expected)    \
    do                                     \
    {                                      \
        const auto r = (result);           \
        ASSERT_FALSE(r.trapped);           \
        ASSERT_EQ(r.stack.size(), 1);      \
        EXPECT_EQ(r.stack[0], (expected)); \
    } while (false)

#define EXPECT_EMPTY_RESULT(result)   \
    do                                \
    {                                 \
        const auto r = (result);      \
        ASSERT_FALSE(r.trapped);      \
        ASSERT_EQ(r.stack.size(), 0); \
    } while (false)

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
    catch (...)                                                                              \
    {                                                                                        \
        ADD_FAILURE() << "Unexpected exception type thrown.";                                \
    }                                                                                        \
    (void)0
