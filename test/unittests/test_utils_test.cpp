// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/hex.hpp>

using namespace fizzy;
using namespace fizzy::test;

TEST(test_utils, hex)
{
    EXPECT_EQ(hex(0x01), "01");
    bytes data = "0102"_bytes;
    EXPECT_EQ(hex(data), "0102");
    EXPECT_EQ(hex(data.data(), data.size()), "0102");
}

TEST(test_utils, from_hex)
{
    EXPECT_EQ(from_hex(""), bytes{});
    // Since the _bytes operator uses from_hex internally, lets check raw bytes first.
    EXPECT_EQ(from_hex("00112233445566778899aabbccddeeffAABBCCDDEEFF"),
        bytes({0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd,
            0xee, 0xff, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff}));
    EXPECT_EQ(from_hex("00112233445566778899aabbccddeeffAABBCCDDEEFF"),
        "00112233445566778899aabbccddeeffAABBCCDDEEFF"_bytes);
    EXPECT_THROW_MESSAGE(from_hex("a"), std::length_error, "the length of the input is odd");
    EXPECT_THROW_MESSAGE(from_hex("aaa"), std::length_error, "the length of the input is odd");
    EXPECT_THROW_MESSAGE(from_hex("gg"), std::out_of_range, "not a hex digit");
    EXPECT_THROW_MESSAGE(from_hex("GG"), std::out_of_range, "not a hex digit");
    EXPECT_THROW_MESSAGE(from_hex("fg"), std::out_of_range, "not a hex digit");
    EXPECT_THROW_MESSAGE(from_hex("FG"), std::out_of_range, "not a hex digit");
}

TEST(test_utils, result_signed_int_typed)
{
    EXPECT_THAT(TypedExecutionResult(Value{-1}, ValType::i32), Result(-1));
    constexpr auto v = std::numeric_limits<int32_t>::min();
    EXPECT_THAT(TypedExecutionResult(Value{v}, ValType::i32), Result(v));
}

TEST(test_utils, print_execution_result)
{
    std::stringstream str_trap;
    str_trap << Trap;
    EXPECT_EQ(str_trap.str(), "trapped");

    std::stringstream str_void;
    str_void << Void;
    EXPECT_EQ(str_void.str(), "result()");

    std::stringstream str_value;
    str_value << ExecutionResult{Value{42_u64}};
    EXPECT_EQ(str_value.str(), "result(42 [0x2a])");
}

TEST(test_utils, print_c_execution_result)
{
    std::stringstream str_trap;
    str_trap << FizzyExecutionResult{true, false, {0}};
    EXPECT_EQ(str_trap.str(), "trapped");

    std::stringstream str_void;
    str_void << FizzyExecutionResult{false, false, {0}};
    EXPECT_EQ(str_void.str(), "result()");

    std::stringstream str_value;
    FizzyValue v;
    v.i64 = 42;
    str_value << FizzyExecutionResult{false, true, v};
    EXPECT_EQ(str_value.str(), "result(42 [0x2a])");
}

TEST(test_utils, print_typed_execution_result)
{
    std::stringstream str_trap;
    str_trap << TypedExecutionResult{Trap, {}};
    EXPECT_EQ(str_trap.str(), "trapped");

    std::stringstream str_void;
    str_void << TypedExecutionResult{Void, {}};
    EXPECT_EQ(str_void.str(), "result()");

    std::stringstream str_value_i32;
    str_value_i32 << TypedExecutionResult{ExecutionResult{Value{42_u32}}, ValType::i32};
    EXPECT_EQ(str_value_i32.str(), "result(42 [0x2a] (i32))");
    str_value_i32.str({});
    str_value_i32 << TypedExecutionResult{ExecutionResult{Value{0x80000000_u32}}, ValType::i32};
    EXPECT_EQ(str_value_i32.str(), "result(2147483648 [0x80000000] (i32))");
    str_value_i32.str({});
    str_value_i32 << TypedExecutionResult{ExecutionResult{Value{-2_u32}}, ValType::i32};
    EXPECT_EQ(str_value_i32.str(), "result(4294967294 [0xfffffffe] (i32))");


    std::stringstream str_value_i64;
    str_value_i64 << TypedExecutionResult{ExecutionResult{Value{42_u64}}, ValType::i64};
    EXPECT_EQ(str_value_i64.str(), "result(42 [0x2a] (i64))");
    str_value_i64.str({});
    str_value_i64 << TypedExecutionResult{ExecutionResult{Value{0x100000000_u64}}, ValType::i64};
    EXPECT_EQ(str_value_i64.str(), "result(4294967296 [0x100000000] (i64))");
    str_value_i64.str({});
    str_value_i64 << TypedExecutionResult{ExecutionResult{Value{-3_u64}}, ValType::i64};
    EXPECT_EQ(str_value_i64.str(), "result(18446744073709551613 [0xfffffffffffffffd] (i64))");

    std::stringstream str_value_f32;
    str_value_f32 << TypedExecutionResult{ExecutionResult{Value{1.125f}}, ValType::f32};
    EXPECT_EQ(str_value_f32.str(), "result(1.125 (f32))");
    str_value_f32.str({});
    str_value_f32 << TypedExecutionResult{ExecutionResult{Value{-1.125f}}, ValType::f32};
    EXPECT_EQ(str_value_f32.str(), "result(-1.125 (f32))");

    std::stringstream str_value_f64;
    str_value_f64 << TypedExecutionResult{ExecutionResult{Value{1.125}}, ValType::f64};
    EXPECT_EQ(str_value_f64.str(), "result(1.125 (f64))");
    str_value_f64.str({});
    str_value_f64 << TypedExecutionResult{ExecutionResult{Value{-1.125}}, ValType::f64};
    EXPECT_EQ(str_value_f64.str(), "result(-1.125 (f64))");
}

TEST(test_utils, result_value_matcher)
{
    // Exercise every check in Result(value) implementation.
    // The implementation and checks below are organized by the value's type in Result(value).
    using testing::Not;

    // TypedExecutionResult is required to be matched against Result(value).
    EXPECT_THAT(ExecutionResult(Value{1_u64}), Not(Result(1_u64)));

    EXPECT_THAT(TypedExecutionResult(fizzy::Void, {}), Not(Result(0)));
    EXPECT_THAT(TypedExecutionResult(fizzy::Trap, {}), Not(Result(0)));

    EXPECT_THAT(TypedExecutionResult(Value{0.0f}, ValType::f32), Result(0.0f));
    EXPECT_THAT(TypedExecutionResult(Value{0.0}, ValType::f64), Not(Result(0.0f)));

    EXPECT_THAT(TypedExecutionResult(Value{0.0}, ValType::f64), Result(0.0));
    EXPECT_THAT(TypedExecutionResult(Value{0.0f}, ValType::f32), Not(Result(0.0)));

    EXPECT_THAT(TypedExecutionResult(Value{0_u64}, ValType::i64), Result(0_u64));
    EXPECT_THAT(TypedExecutionResult(Value{0_u32}, ValType::i32), Not(Result(0_u64)));

    EXPECT_THAT(TypedExecutionResult(Value{0_u32}, ValType::i32), Result(0_u32));

    // For non-negative values zero-extension is conveniently allowed.
    EXPECT_THAT(TypedExecutionResult(Value{0_u64}, ValType::i64), Result(0));
    EXPECT_THAT(TypedExecutionResult(Value{0_u64}, ValType::i64), Result(0_u32));

    EXPECT_THAT(TypedExecutionResult(Value{-1_u32}, ValType::i32), Result(-1));
    EXPECT_THAT(TypedExecutionResult(Value{-1_u32}, ValType::i32), Result(-1_u32));
    EXPECT_THAT(TypedExecutionResult(Value{-1_u32}, ValType::i32), Not(Result(-1_u64)));

    EXPECT_THAT(TypedExecutionResult(Value{-1_u64}, ValType::i64), Result(-1_u64));
    EXPECT_THAT(TypedExecutionResult(Value{-1_u64}, ValType::i64), Not(Result(-1)));
    EXPECT_THAT(TypedExecutionResult(Value{-1_u64}, ValType::i64), Not(Result(-1_u32)));

    // Comparing with non-wasm types always return false.
    EXPECT_THAT(TypedExecutionResult(Value{1_u32}, ValType::i32), Not(Result(uint8_t{1})));
    EXPECT_THAT(TypedExecutionResult(Value{1_u64}, ValType::i64), Not(Result(uint8_t{1})));
}

TEST(test_utils, result_value_matcher_explain_missing_result_type)
{
    const auto result = testing::internal::MakePredicateFormatterFromMatcher(Result(1_u64))(
        "<value>", ExecutionResult(Value{1_u64}));
    EXPECT_FALSE(result);
    EXPECT_STREQ(result.message(),
        "Value of: <value>\n"
        "Expected: result 1\n"
        "  Actual: result(1 [0x1]) (of type fizzy::ExecutionResult), "
        "TypedExecutionResult expected");
}

TEST(test_utils, result_value_matcher_explain_non_wasm_type)
{
    const auto result = testing::internal::MakePredicateFormatterFromMatcher(Result(char{1}))(
        "<value>", TypedExecutionResult(Value{1_u32}, ValType::i32));
    EXPECT_FALSE(result);
    EXPECT_STREQ(result.message(),
        "Value of: <value>\n"
        "Expected: result '\\x1' (1)\n"
        "  Actual: result(1 [0x1] (i32)) (of type fizzy::test::TypedExecutionResult), "
        "expected value has non-wasm type");
}
