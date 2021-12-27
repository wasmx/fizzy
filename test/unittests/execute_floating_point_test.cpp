// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute_floating_point_test.hpp"
#include "execute.hpp"
#include "parser.hpp"
#include "trunc_boundaries.hpp"
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <cmath>

using namespace fizzy;
using namespace fizzy::test;

TEST(execute_floating_point, f32_const)
{
    /* wat2wasm
    (func (result f32)
      f32.const 4194304.1
    )
    */
    const auto wasm = from_hex("0061736d010000000105016000017d030201000a09010700430000804a0b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Result(4194304.1f));
}

TEST(execute_floating_point, f64_const)
{
    /* wat2wasm
    (func (result f64)
      f64.const 8589934592.1
    )
    */
    const auto wasm =
        from_hex("0061736d010000000105016000017c030201000a0d010b0044cdcc0000000000420b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {}), Result(8589934592.1));
}

TYPED_TEST(execute_floating_point_types, test_values_selftest)
{
    using TV = TestValues<TypeParam>;

    EXPECT_EQ(std::size(TV::positive_nonzero_infinite()), TV::num_positive - 1);
    EXPECT_EQ(std::size(TV::positive_nonzero_finite()), TV::num_positive - 2);
    EXPECT_EQ(std::size(TV::positive_nans()), TV::num_nans);
    EXPECT_EQ(std::size(TV::positive_noncanonical_nans()), TV::num_nans - 1);
    EXPECT_EQ(std::size(TV::positive_all()), TV::num_positive + TV::num_nans);

    for (const auto nan : TV::positive_nans())
        EXPECT_TRUE(std::isnan(nan));

    EXPECT_TRUE(FP<TypeParam>{*TV::canonical_nan}.is_canonical_nan());

    for (const auto nan : TV::positive_noncanonical_nans())
    {
        EXPECT_TRUE(std::isnan(nan));
        EXPECT_FALSE(FP<TypeParam>{nan}.is_canonical_nan());
    }

    for (const auto p : TV::positive_all())
        EXPECT_EQ(std::signbit(p), 0);

    const auto& ordered = TV::ordered();
    auto current = ordered[0];
    EXPECT_EQ(current, -FP<TypeParam>::Limits::infinity());
    for (size_t i = 1; i < ordered.size(); ++i)
    {
        EXPECT_LT(current, ordered[i]);
        current = ordered[i];
    }

    const auto& ordered_and_nans = TV::ordered_and_nans();
    current = ordered_and_nans[0];
    EXPECT_EQ(current, -FP<TypeParam>::Limits::infinity());
    size_t nan_start_index = 0;
    for (size_t i = 1; i < ordered_and_nans.size(); ++i)
    {
        if (std::isnan(ordered_and_nans[i]))
        {
            nan_start_index = i;
            break;
        }

        EXPECT_LT(current, ordered_and_nans[i]);
        current = ordered_and_nans[i];
    }
    ASSERT_NE(nan_start_index, 0);
    for (size_t i = nan_start_index; i < ordered_and_nans.size(); ++i)
    {
        EXPECT_TRUE(std::isnan(ordered_and_nans[i]));
    }
}

TYPED_TEST(execute_floating_point_types, nan_matchers)
{
    using testing::Not;
    using FP = FP<TypeParam>;

    EXPECT_THAT(Void, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(Trap, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{TypeParam{}}}, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon)}}, CanonicalNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{-FP::nan(FP::canon)}}, CanonicalNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon + 1)}}, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{-FP::nan(FP::canon + 1)}}, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(1)}}, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{-FP::nan(1)}}, Not(CanonicalNaN(TypeParam{})));

    EXPECT_THAT(Void, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(Trap, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{TypeParam{}}}, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon)}}, ArithmeticNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{-FP::nan(FP::canon)}}, ArithmeticNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon + 1)}}, ArithmeticNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{-FP::nan(FP::canon + 1)}}, ArithmeticNaN(TypeParam{}));

#if SNAN_SUPPORTED
    EXPECT_THAT(ExecutionResult{Value{FP::nan(1)}}, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{-FP::nan(1)}}, Not(ArithmeticNaN(TypeParam{})));
#endif
}

TYPED_TEST(execute_floating_point_types, unop_nan_propagation)
{
    // Tests NaN propagation in unary instructions (unop).
    // If NaN input is canonical NN, the result must be the canonical NaN.
    // Otherwise, the result must be an arithmetic NaN.

    // The list of instructions to be tested.
    // Only f32 variants, but f64 variants are going to be covered as well.
    constexpr Instr opcodes[] = {
        Instr::f32_ceil,
        Instr::f32_floor,
        Instr::f32_trunc,
        Instr::f32_nearest,
        Instr::f32_sqrt,
    };

    for (const auto op : opcodes)
    {
        auto instance = instantiate(parse(this->get_unop_code(op)));

        const auto cnan = FP<TypeParam>::nan(FP<TypeParam>::canon);

        ASSERT_EQ(std::fegetround(), FE_TONEAREST);
        for (const auto rounding_direction : all_rounding_directions)
        {
            ASSERT_EQ(std::fesetround(rounding_direction), 0);
            SCOPED_TRACE(rounding_direction);

            EXPECT_THAT(execute(*instance, 0, {cnan}), CanonicalNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {-cnan}), CanonicalNaN(TypeParam{}));

            for (const auto nan : TestValues<TypeParam>::positive_noncanonical_nans())
            {
                const auto res1 = execute(*instance, 0, {nan});
                EXPECT_THAT(res1, ArithmeticNaN(TypeParam{}))
                    << std::hex << FP<TypeParam>{res1.value.template as<TypeParam>()}.nan_payload();
                const auto res2 = execute(*instance, 0, {-nan});
                EXPECT_THAT(res2, ArithmeticNaN(TypeParam{}))
                    << std::hex << FP<TypeParam>{res2.value.template as<TypeParam>()}.nan_payload();
            }
        }
        ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
    }
}

TYPED_TEST(execute_floating_point_types, binop_nan_propagation)
{
    // Tests NaN propagation in binary instructions (binop).
    // If all NaN inputs are canonical the result must be the canonical NaN.
    // Otherwise, the result must be an arithmetic NaN.

    // The list of instructions to be tested.
    // Only f32 variants, but f64 variants are going to be covered as well.
    constexpr Instr opcodes[] = {
        Instr::f32_add,
        Instr::f32_sub,
        Instr::f32_mul,
        Instr::f32_div,
        Instr::f32_min,
        Instr::f32_max,
    };

    for (const auto op : opcodes)
    {
        auto instance = instantiate(parse(this->get_binop_code(op)));
        const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };

        constexpr auto q = TypeParam{1.0};
        const auto cnan = FP<TypeParam>::nan(FP<TypeParam>::canon);

        // TODO: Consider more restrictive tests where the sign of NaN values is also checked.

        EXPECT_THAT(exec(q, cnan), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(exec(q, -cnan), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(exec(cnan, q), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(exec(-cnan, q), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(exec(cnan, cnan), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(exec(cnan, -cnan), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(exec(-cnan, cnan), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(exec(-cnan, -cnan), CanonicalNaN(TypeParam{}));

        for (const auto nan : TestValues<TypeParam>::positive_noncanonical_nans())
        {
            EXPECT_THAT(exec(q, nan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(q, -nan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(nan, q), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(-nan, q), ArithmeticNaN(TypeParam{}));

            EXPECT_THAT(exec(nan, nan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(nan, -nan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(-nan, nan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(-nan, -nan), ArithmeticNaN(TypeParam{}));

            EXPECT_THAT(exec(nan, cnan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(nan, -cnan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(-nan, cnan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(-nan, -cnan), ArithmeticNaN(TypeParam{}));

            EXPECT_THAT(exec(cnan, nan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(cnan, -nan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(-cnan, nan), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(exec(-cnan, -nan), ArithmeticNaN(TypeParam{}));
        }
    }
}

TYPED_TEST(execute_floating_point_types, compare)
{
    /* wat2wasm
    (func (param f32 f32) (result i32) (f32.eq (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.ne (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.lt (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.gt (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.le (local.get 0) (local.get 1)))
    (func (param f32 f32) (result i32) (f32.ge (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.eq (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.ne (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.lt (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.gt (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.le (local.get 0) (local.get 1)))
    (func (param f64 f64) (result i32) (f64.ge (local.get 0) (local.get 1)))
    */
    const auto wasm = from_hex(
        "0061736d01000000010d0260027d7d017f60027c7c017f030d0c0000000000000101010101010a610c07002000"
        "20015b0b0700200020015c0b0700200020015d0b0700200020015e0b0700200020015f0b070020002001600b07"
        "0020002001610b070020002001620b070020002001630b070020002001640b070020002001650b070020002001"
        "660b");
    auto instance = instantiate(parse(wasm));

    constexpr FuncIdx func_offset = std::is_same_v<TypeParam, float> ? 0 : 6;
    const auto eq = [&](auto a, auto b) { return execute(*instance, func_offset + 0, {a, b}); };
    const auto ne = [&](auto a, auto b) { return execute(*instance, func_offset + 1, {a, b}); };
    const auto lt = [&](auto a, auto b) { return execute(*instance, func_offset + 2, {a, b}); };
    const auto gt = [&](auto a, auto b) { return execute(*instance, func_offset + 3, {a, b}); };
    const auto le = [&](auto a, auto b) { return execute(*instance, func_offset + 4, {a, b}); };
    const auto ge = [&](auto a, auto b) { return execute(*instance, func_offset + 5, {a, b}); };

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        // Check every pair from cartesian product of ordered_values.
        const auto& ordered_values = TestValues<TypeParam>::ordered_and_nans();
        for (size_t i = 0; i < std::size(ordered_values); ++i)
        {
            for (size_t j = 0; j < std::size(ordered_values); ++j)
            {
                const auto a = ordered_values[i];
                const auto b = ordered_values[j];
                if (std::isnan(a) || std::isnan(b))
                {
                    EXPECT_THAT(eq(a, b), Result(0)) << a << "==" << b;
                    EXPECT_THAT(ne(a, b), Result(1)) << a << "!=" << b;
                    EXPECT_THAT(lt(a, b), Result(0)) << a << "<" << b;
                    EXPECT_THAT(gt(a, b), Result(0)) << a << ">" << b;
                    EXPECT_THAT(le(a, b), Result(0)) << a << "<=" << b;
                    EXPECT_THAT(ge(a, b), Result(0)) << a << ">=" << b;
                }
                else
                {
                    EXPECT_THAT(eq(a, b), Result(uint32_t{i == j})) << a << "==" << b;
                    EXPECT_THAT(ne(a, b), Result(uint32_t{i != j})) << a << "!=" << b;
                    EXPECT_THAT(lt(a, b), Result(uint32_t{i < j})) << a << "<" << b;
                    EXPECT_THAT(gt(a, b), Result(uint32_t{i > j})) << a << ">" << b;
                    EXPECT_THAT(le(a, b), Result(uint32_t{i <= j})) << a << "<=" << b;
                    EXPECT_THAT(ge(a, b), Result(uint32_t{i >= j})) << a << ">=" << b;
                }
            }
        }

        // Negative zero. This is separate set of checks because -0.0 cannot be placed
        // in the ordered_values array as -0.0 == 0.0.
        EXPECT_THAT(eq(TypeParam{-0.0}, TypeParam{0.0}), Result(1));
        EXPECT_THAT(ne(TypeParam{-0.0}, TypeParam{0.0}), Result(0));
        EXPECT_THAT(lt(TypeParam{-0.0}, TypeParam{0.0}), Result(0));
        EXPECT_THAT(gt(TypeParam{-0.0}, TypeParam{0.0}), Result(0));
        EXPECT_THAT(le(TypeParam{-0.0}, TypeParam{0.0}), Result(1));
        EXPECT_THAT(ge(TypeParam{-0.0}, TypeParam{0.0}), Result(1));
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}

TYPED_TEST(execute_floating_point_types, abs)
{
    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_abs)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        for (const auto p : TestValues<TypeParam>::positive_all())
        {
            // fabs(+-p) = +p
            EXPECT_THAT(exec(p), Result(p));
            EXPECT_THAT(exec(-p), Result(p));
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}

TYPED_TEST(execute_floating_point_types, neg)
{
    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_neg)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);
        for (const auto p : TestValues<TypeParam>::positive_all())
        {
            // fneg(+-p) = -+p
            EXPECT_THAT(exec(p), Result(-p));
            EXPECT_THAT(exec(-p), Result(p));
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}

TYPED_TEST(execute_floating_point_types, ceil)
{
    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_ceil)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        // fceil(-q) = -0  (if -1 < -q < 0)
        // (also included in positive_trunc_tests, here for explicitness).
        EXPECT_THAT(exec(std::nextafter(-TypeParam{1}, TypeParam{0})), Result(-TypeParam{0}));
        EXPECT_THAT(exec(-FP<TypeParam>::Limits::denorm_min()), Result(-TypeParam{0}));

        for (const auto& [arg, expected_trunc] :
            execute_floating_point_types<TypeParam>::positive_trunc_tests)
        {
            // For positive values, the ceil() is trunc() + 1,
            // unless the input is already an integer.
            const auto expected_positive =
                (arg == expected_trunc) ? expected_trunc : expected_trunc + TypeParam{1};
            EXPECT_THAT(exec(arg), Result(expected_positive)) << arg << ": " << expected_positive;

            // For negative values, the ceil() is trunc().
            EXPECT_THAT(exec(-arg), Result(-expected_trunc)) << -arg << ": " << -expected_trunc;
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}

TYPED_TEST(execute_floating_point_types, floor)
{
    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_floor)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        // ffloor(+q) = +0  (if 0 < +q < 1)
        // (also included in positive_trunc_tests, here for explicitness).
        EXPECT_THAT(exec(FP<TypeParam>::Limits::denorm_min()), Result(TypeParam{0}));
        EXPECT_THAT(exec(std::nextafter(TypeParam{1}, TypeParam{0})), Result(TypeParam{0}));

        for (const auto& [arg, expected_trunc] :
            execute_floating_point_types<TypeParam>::positive_trunc_tests)
        {
            // For positive values, the floor() is trunc().
            EXPECT_THAT(exec(arg), Result(expected_trunc)) << arg << ": " << expected_trunc;

            // For negative values, the floor() is trunc() - 1, unless the input is already an
            // integer.
            const auto expected_negative =
                (arg == expected_trunc) ? -expected_trunc : -expected_trunc - TypeParam{1};
            EXPECT_THAT(exec(-arg), Result(expected_negative)) << -arg << ": " << expected_negative;
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}

TYPED_TEST(execute_floating_point_types, trunc)
{
    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_trunc)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        // ftrunc(+q) = +0  (if 0 < +q < 1)
        // (also included in positive_trunc_tests, here for explicitness).
        EXPECT_THAT(exec(FP<TypeParam>::Limits::denorm_min()), Result(TypeParam{0}));
        EXPECT_THAT(exec(std::nextafter(TypeParam{1}, TypeParam{0})), Result(TypeParam{0}));

        // ftrunc(-q) = -0  (if -1 < -q < 0)
        // (also included in positive_trunc_tests, here for explicitness).
        EXPECT_THAT(exec(std::nextafter(-TypeParam{1}, TypeParam{0})), Result(-TypeParam{0}));
        EXPECT_THAT(exec(-FP<TypeParam>::Limits::denorm_min()), Result(-TypeParam{0}));

        for (const auto& [arg, expected] :
            execute_floating_point_types<TypeParam>::positive_trunc_tests)
        {
            EXPECT_THAT(exec(arg), Result(expected)) << arg << ": " << expected;
            EXPECT_THAT(exec(-arg), Result(-expected)) << -arg << ": " << -expected;
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}

TYPED_TEST(execute_floating_point_types, nearest)
{
    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_nearest)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        // fnearest(+q) = +0  (if 0 < +q <= 0.5)
        // (also included in positive_trunc_tests, here for explicitness).
        EXPECT_THAT(exec(FP<TypeParam>::Limits::denorm_min()), Result(TypeParam{0}));
        EXPECT_THAT(exec(std::nextafter(TypeParam{0.5}, TypeParam{0})), Result(TypeParam{0}));
        EXPECT_THAT(exec(TypeParam{0.5}), Result(TypeParam{0}));

        // fnearest(-q) = -0  (if -0.5 <= -q < 0)
        // (also included in positive_trunc_tests, here for explicitness).
        EXPECT_THAT(exec(-TypeParam{0.5}), Result(-TypeParam{0}));
        EXPECT_THAT(exec(std::nextafter(-TypeParam{0.5}, TypeParam{0})), Result(-TypeParam{0}));
        EXPECT_THAT(exec(-FP<TypeParam>::Limits::denorm_min()), Result(-TypeParam{0}));

        // This checks all other specification-driven test cases, including:
        // fnearest(+-inf) = +-inf
        // fnearest(+-0) = +-0
        for (const auto& [arg, expected_trunc] : this->positive_trunc_tests)
        {
            ASSERT_EQ(expected_trunc, std::trunc(expected_trunc));
            const auto is_even = std::fmod(expected_trunc, TypeParam{2}) == TypeParam{0};

            // Computing "expected" is expanded to individual cases to check code coverage.
            TypeParam expected;
            if (arg == expected_trunc)
            {
                expected = expected_trunc;
            }
            else
            {
                const auto diff = arg - expected_trunc;
                ASSERT_LT(diff, TypeParam{1});
                ASSERT_GT(diff, TypeParam{0});

                if (diff < TypeParam{0.5})
                    expected = expected_trunc;  // NOLINT(bugprone-branch-clone)
                else if (diff > TypeParam{0.5})
                    expected = expected_trunc + TypeParam{1};  // NOLINT(bugprone-branch-clone)
                else if (is_even)
                    expected = expected_trunc;
                else
                    expected = expected_trunc + TypeParam{1};
            }

            EXPECT_THAT(exec(arg), Result(expected)) << arg << ": " << expected;
            EXPECT_THAT(exec(-arg), Result(-expected)) << -arg << ": " << -expected;
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}


TYPED_TEST(execute_floating_point_types, sqrt)
{
    using FP = FP<TypeParam>;
    using Limits = typename FP::Limits;

    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_sqrt)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    // fsqrt(-inf) = nan:canonical
    EXPECT_THAT(exec(-Limits::infinity()), CanonicalNaN(TypeParam{}));

    // fsqrt(+inf) = +inf
    EXPECT_THAT(exec(Limits::infinity()), Result(Limits::infinity()));

    // fsqrt(+-0) = +-0
    EXPECT_THAT(exec(TypeParam{0.0}), Result(TypeParam{0.0}));
    EXPECT_THAT(exec(-TypeParam{0.0}), Result(-TypeParam{0.0}));

    for (const auto p : TestValues<TypeParam>::positive_nonzero_finite())
    {
        // fsqrt(-p) = nan:canonical
        EXPECT_THAT(exec(-p), CanonicalNaN(TypeParam{}));
    }

    EXPECT_THAT(exec(TypeParam{1}), Result(TypeParam{1}));
    EXPECT_THAT(exec(TypeParam{4}), Result(TypeParam{2}));
    EXPECT_THAT(exec(TypeParam{0x1.21p6}), Result(TypeParam{0x1.1p3}));
}

TYPED_TEST(execute_floating_point_types, add)
{
    using FP = FP<TypeParam>;
    using Limits = typename FP::Limits;

    auto instance = instantiate(parse(this->get_binop_code(Instr::f32_add)));
    const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };

    // fadd(+-inf, -+inf) = nan:canonical
    EXPECT_THAT(exec(Limits::infinity(), -Limits::infinity()), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-Limits::infinity(), Limits::infinity()), CanonicalNaN(TypeParam{}));

    // fadd(+-inf, +-inf) = +-inf
    EXPECT_THAT(exec(Limits::infinity(), Limits::infinity()), Result(Limits::infinity()));
    EXPECT_THAT(exec(-Limits::infinity(), -Limits::infinity()), Result(-Limits::infinity()));

    // fadd(+-0, -+0) = +0
    EXPECT_THAT(exec(TypeParam{0.0}, -TypeParam{0.0}), Result(TypeParam{0.0}));
    EXPECT_THAT(exec(-TypeParam{0.0}, TypeParam{0.0}), Result(TypeParam{0.0}));

    // fadd(+-0, +-0) = +-0
    EXPECT_THAT(exec(TypeParam{0.0}, TypeParam{0.0}), Result(TypeParam{0.0}));
    EXPECT_THAT(exec(-TypeParam{0.0}, -TypeParam{0.0}), Result(-TypeParam{0.0}));

    // fadd(z1, +-0) = z1  (for z1 = +-inf)
    EXPECT_THAT(exec(Limits::infinity(), TypeParam{0.0}), Result(Limits::infinity()));
    EXPECT_THAT(exec(Limits::infinity(), -TypeParam{0.0}), Result(Limits::infinity()));
    EXPECT_THAT(exec(-Limits::infinity(), TypeParam{0.0}), Result(-Limits::infinity()));
    EXPECT_THAT(exec(-Limits::infinity(), -TypeParam{0.0}), Result(-Limits::infinity()));

    // fadd(+-0, z2) = z2  (for z2 = +-inf)
    EXPECT_THAT(exec(TypeParam{0.0}, Limits::infinity()), Result(Limits::infinity()));
    EXPECT_THAT(exec(TypeParam{0.0}, -Limits::infinity()), Result(-Limits::infinity()));
    EXPECT_THAT(exec(-TypeParam{0.0}, Limits::infinity()), Result(Limits::infinity()));
    EXPECT_THAT(exec(-TypeParam{0.0}, -Limits::infinity()), Result(-Limits::infinity()));

    for (const auto q : TestValues<TypeParam>::positive_nonzero_finite())
    {
        // fadd(z1, +-inf) = +-inf  (for z1 = +-q)
        EXPECT_THAT(exec(q, Limits::infinity()), Result(Limits::infinity()));
        EXPECT_THAT(exec(q, -Limits::infinity()), Result(-Limits::infinity()));
        EXPECT_THAT(exec(-q, Limits::infinity()), Result(Limits::infinity()));
        EXPECT_THAT(exec(-q, -Limits::infinity()), Result(-Limits::infinity()));

        // fadd(+-inf, z2) = +-inf  (for z2 = +-q)
        EXPECT_THAT(exec(Limits::infinity(), q), Result(Limits::infinity()));
        EXPECT_THAT(exec(-Limits::infinity(), q), Result(-Limits::infinity()));
        EXPECT_THAT(exec(Limits::infinity(), -q), Result(Limits::infinity()));
        EXPECT_THAT(exec(-Limits::infinity(), -q), Result(-Limits::infinity()));

        // fadd(z1, +-0) = z1  (for z1 = +-q)
        EXPECT_THAT(exec(q, TypeParam{0.0}), Result(q));
        EXPECT_THAT(exec(q, -TypeParam{0.0}), Result(q));
        EXPECT_THAT(exec(-q, TypeParam{0.0}), Result(-q));
        EXPECT_THAT(exec(-q, -TypeParam{0.0}), Result(-q));

        // fadd(+-0, z2) = z2  (for z2 = +-q)
        EXPECT_THAT(exec(TypeParam{0.0}, q), Result(q));
        EXPECT_THAT(exec(-TypeParam{0.0}, q), Result(q));
        EXPECT_THAT(exec(TypeParam{0.0}, -q), Result(-q));
        EXPECT_THAT(exec(-TypeParam{0.0}, -q), Result(-q));

        // fadd(+-q, -+q) = +0
        EXPECT_THAT(exec(q, -q), Result(TypeParam{0.0}));
        EXPECT_THAT(exec(-q, q), Result(TypeParam{0.0}));
    }

    EXPECT_THAT(exec(TypeParam{0x0.287p2}, TypeParam{0x1.FFp4}), Result(TypeParam{0x1.048Ep5}));
}

TYPED_TEST(execute_floating_point_types, sub)
{
    using FP = FP<TypeParam>;
    using Limits = typename FP::Limits;

    auto instance = instantiate(parse(this->get_binop_code(Instr::f32_sub)));
    const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };

    // fsub(+-inf, +-inf) = nan:canonical
    EXPECT_THAT(exec(Limits::infinity(), Limits::infinity()), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-Limits::infinity(), -Limits::infinity()), CanonicalNaN(TypeParam{}));

    // fsub(+-inf, -+inf) = +-inf
    EXPECT_THAT(exec(Limits::infinity(), -Limits::infinity()), Result(Limits::infinity()));
    EXPECT_THAT(exec(-Limits::infinity(), Limits::infinity()), Result(-Limits::infinity()));

    // fsub(+-0, +-0) = +0
    EXPECT_THAT(exec(TypeParam{0.0}, TypeParam{0.0}), Result(TypeParam{0.0}));
    EXPECT_THAT(exec(-TypeParam{0.0}, -TypeParam{0.0}), Result(TypeParam{0.0}));

    // fsub(+-0, -+0) = +-0
    EXPECT_THAT(exec(TypeParam{0.0}, -TypeParam{0.0}), Result(TypeParam{0.0}));
    EXPECT_THAT(exec(-TypeParam{0.0}, TypeParam{0.0}), Result(-TypeParam{0.0}));

    // fsub(z1, +-0) = z1  (for z1 = +-inf)
    EXPECT_THAT(exec(Limits::infinity(), TypeParam{0.0}), Result(Limits::infinity()));
    EXPECT_THAT(exec(Limits::infinity(), -TypeParam{0.0}), Result(Limits::infinity()));
    EXPECT_THAT(exec(-Limits::infinity(), TypeParam{0.0}), Result(-Limits::infinity()));
    EXPECT_THAT(exec(-Limits::infinity(), -TypeParam{0.0}), Result(-Limits::infinity()));

    for (const auto q : TestValues<TypeParam>::positive_nonzero_finite())
    {
        // fsub(z1, +-inf) = -+inf  (for z1 = +-q)
        EXPECT_THAT(exec(q, Limits::infinity()), Result(-Limits::infinity()));
        EXPECT_THAT(exec(q, -Limits::infinity()), Result(Limits::infinity()));
        EXPECT_THAT(exec(-q, Limits::infinity()), Result(-Limits::infinity()));
        EXPECT_THAT(exec(-q, -Limits::infinity()), Result(Limits::infinity()));

        // fsub(+-inf, z2) = +-inf  (for z2 = +-q)
        EXPECT_THAT(exec(Limits::infinity(), q), Result(Limits::infinity()));
        EXPECT_THAT(exec(-Limits::infinity(), q), Result(-Limits::infinity()));
        EXPECT_THAT(exec(Limits::infinity(), -q), Result(Limits::infinity()));
        EXPECT_THAT(exec(-Limits::infinity(), -q), Result(-Limits::infinity()));

        // fsub(z1, +-0) = z1  (for z1 = +-q)
        EXPECT_THAT(exec(q, TypeParam{0.0}), Result(q));
        EXPECT_THAT(exec(q, -TypeParam{0.0}), Result(q));
        EXPECT_THAT(exec(-q, TypeParam{0.0}), Result(-q));
        EXPECT_THAT(exec(-q, -TypeParam{0.0}), Result(-q));

        // fsub(+-0, +-q2) = -+q2
        EXPECT_THAT(exec(TypeParam{0.0}, q), Result(-q));
        EXPECT_THAT(exec(-TypeParam{0.0}, -q), Result(q));

        // Not part of the spec.
        EXPECT_THAT(exec(TypeParam{0.0}, -q), Result(q));
        EXPECT_THAT(exec(-TypeParam{0.0}, q), Result(-q));

        // fsub(+-q, +-q) = +0
        EXPECT_THAT(exec(q, q), Result(TypeParam{0.0}));
        EXPECT_THAT(exec(-q, -q), Result(TypeParam{0.0}));
    }

    EXPECT_THAT(exec(TypeParam{0x1.048Ep5}, TypeParam{0x1.FFp4}), Result(TypeParam{0x0.287p2}));
}

TYPED_TEST(execute_floating_point_types, add_sub_neg_relation)
{
    // Checks the note from the Wasm spec:
    // > Up to the non-determinism regarding NaNs, it always holds that
    // > fsub(z1, z2) = fadd(z1, fneg(z2)).

    using FP = FP<TypeParam>;

    /* wat2wasm
    (func (param f32 f32) (result f32) (f32.sub (local.get 0) (local.get 1)))
    (func (param f32 f32) (result f32) (f32.add (local.get 0) (f32.neg(local.get 1))))

    (func (param f64 f64) (result f64) (f64.sub (local.get 0) (local.get 1)))
    (func (param f64 f64) (result f64) (f64.add (local.get 0) (f64.neg(local.get 1))))
    */
    const auto wasm = from_hex(
        "0061736d01000000010d0260027d7d017d60027c7c017c030504000001010a2304070020002001930b08002000"
        "20018c920b070020002001a10b0800200020019aa00b");

    auto module = parse(wasm);
    constexpr auto fn_offset = std::is_same_v<TypeParam, float> ? 0 : 2;
    constexpr auto sub_fn_idx = 0 + fn_offset;
    constexpr auto addneg_fn_idx = 1 + fn_offset;

    auto instance = instantiate(std::move(module));
    const auto sub = [&](auto a, auto b) { return execute(*instance, sub_fn_idx, {a, b}); };
    const auto addneg = [&](auto a, auto b) { return execute(*instance, addneg_fn_idx, {a, b}); };

    const auto& ordered_values = TestValues<TypeParam>::ordered_and_nans();
    for (const auto z1 : ordered_values)
    {
        for (const auto z2 : ordered_values)
        {
            const auto sub_result = sub(z1, z2);
            ASSERT_TRUE(sub_result.has_value);
            const auto addneg_result = addneg(z1, z2);
            ASSERT_TRUE(addneg_result.has_value);

            if (std::isnan(z1) || std::isnan(z2))
            {
                if (FP{z1}.nan_payload() == FP::canon && FP{z2}.nan_payload() == FP::canon)
                {
                    EXPECT_THAT(sub_result, CanonicalNaN(TypeParam{}));
                    EXPECT_THAT(addneg_result, CanonicalNaN(TypeParam{}));
                }
                else
                {
                    EXPECT_THAT(sub_result, ArithmeticNaN(TypeParam{}));
                    EXPECT_THAT(addneg_result, ArithmeticNaN(TypeParam{}));
                }
            }
            else
            {
                EXPECT_THAT(addneg_result, Result(sub_result.value.template as<TypeParam>()));
            }
        }
    }
}

TYPED_TEST(execute_floating_point_types, mul)
{
    using FP = FP<TypeParam>;
    using Limits = typename FP::Limits;

    auto instance = instantiate(parse(this->get_binop_code(Instr::f32_mul)));
    const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };

    // fmul(+-inf, +-0) = nan:canonical
    EXPECT_THAT(exec(Limits::infinity(), TypeParam{0.0}), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-Limits::infinity(), -TypeParam{0.0}), CanonicalNaN(TypeParam{}));

    // fmul(+-inf, -+0) = nan:canonical
    EXPECT_THAT(exec(Limits::infinity(), -TypeParam{0.0}), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-Limits::infinity(), TypeParam{0.0}), CanonicalNaN(TypeParam{}));

    // fmul(+-0, +-inf) = nan:canonical
    EXPECT_THAT(exec(TypeParam{0.0}, Limits::infinity()), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-TypeParam{0.0}, -Limits::infinity()), CanonicalNaN(TypeParam{}));

    // fmul(+-0, -+inf) = nan:canonical
    EXPECT_THAT(exec(TypeParam{0.0}, -Limits::infinity()), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-TypeParam{0.0}, Limits::infinity()), CanonicalNaN(TypeParam{}));

    // fmul(+-inf, +-inf) = +inf
    EXPECT_THAT(exec(Limits::infinity(), Limits::infinity()), Result(Limits::infinity()));
    EXPECT_THAT(exec(-Limits::infinity(), -Limits::infinity()), Result(Limits::infinity()));

    // fmul(+-inf, -+inf) = -inf
    EXPECT_THAT(exec(Limits::infinity(), -Limits::infinity()), Result(-Limits::infinity()));
    EXPECT_THAT(exec(-Limits::infinity(), Limits::infinity()), Result(-Limits::infinity()));

    // fmul(+-0, +-0) = +0
    EXPECT_THAT(exec(TypeParam{0.0}, TypeParam{0.0}), Result(TypeParam{0.0}));
    EXPECT_THAT(exec(-TypeParam{0.0}, -TypeParam{0.0}), Result(TypeParam{0.0}));

    // fmul(+-0, -+0) = -0
    EXPECT_THAT(exec(TypeParam{0.0}, -TypeParam{0.0}), Result(-TypeParam{0.0}));
    EXPECT_THAT(exec(-TypeParam{0.0}, TypeParam{0.0}), Result(-TypeParam{0.0}));

    for (const auto q : TestValues<TypeParam>::positive_nonzero_finite())
    {
        // fmul(+-q1, +-inf) = +inf
        EXPECT_THAT(exec(q, Limits::infinity()), Result(Limits::infinity()));
        EXPECT_THAT(exec(-q, -Limits::infinity()), Result(Limits::infinity()));

        // fmul(+-q1, -+inf) = -inf
        EXPECT_THAT(exec(q, -Limits::infinity()), Result(-Limits::infinity()));
        EXPECT_THAT(exec(-q, Limits::infinity()), Result(-Limits::infinity()));

        // fmul(+-inf, +-q2) = +inf
        EXPECT_THAT(exec(Limits::infinity(), q), Result(Limits::infinity()));
        EXPECT_THAT(exec(-Limits::infinity(), -q), Result(Limits::infinity()));

        // fmul(+-inf, -+q2) = -inf
        EXPECT_THAT(exec(-Limits::infinity(), q), Result(-Limits::infinity()));
        EXPECT_THAT(exec(Limits::infinity(), -q), Result(-Limits::infinity()));
    }

    EXPECT_THAT(exec(TypeParam{0x0.287p2}, TypeParam{0x1.FFp4}), Result(TypeParam{0x1.42DE4p4}));

    const auto _1p = std::nextafter(TypeParam{1.0}, Limits::max());
    EXPECT_THAT(exec(Limits::max(), _1p), Result(Limits::infinity()));
    EXPECT_THAT(exec(-Limits::max(), -_1p), Result(Limits::infinity()));
    EXPECT_THAT(exec(Limits::max(), -_1p), Result(-Limits::infinity()));
    EXPECT_THAT(exec(-Limits::max(), _1p), Result(-Limits::infinity()));
}

TYPED_TEST(execute_floating_point_types, div)
{
    using FP = FP<TypeParam>;
    using Limits = typename FP::Limits;

    auto instance = instantiate(parse(this->get_binop_code(Instr::f32_div)));
    const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };

    // fdiv(+-inf, +-inf) = nan:canonical
    EXPECT_THAT(exec(Limits::infinity(), Limits::infinity()), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-Limits::infinity(), -Limits::infinity()), CanonicalNaN(TypeParam{}));

    // fdiv(+-inf, -+inf) = nan:canonical
    EXPECT_THAT(exec(Limits::infinity(), -Limits::infinity()), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-Limits::infinity(), Limits::infinity()), CanonicalNaN(TypeParam{}));

    // fdiv(+-0, +-0) = nan:canonical
    EXPECT_THAT(exec(TypeParam{0.0}, TypeParam{0.0}), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-TypeParam{0.0}, -TypeParam{0.0}), CanonicalNaN(TypeParam{}));

    // fdiv(+-0, -+0) = nan:canonical
    EXPECT_THAT(exec(TypeParam{0.0}, -TypeParam{0.0}), CanonicalNaN(TypeParam{}));
    EXPECT_THAT(exec(-TypeParam{0.0}, TypeParam{0.0}), CanonicalNaN(TypeParam{}));

    for (const auto q : TestValues<TypeParam>::positive_nonzero_finite())
    {
        // fdiv(+-inf, +-q2) = +inf
        EXPECT_THAT(exec(Limits::infinity(), q), Result(Limits::infinity()));
        EXPECT_THAT(exec(-Limits::infinity(), -q), Result(Limits::infinity()));

        // fdiv(+-inf, -+q2) = -inf
        EXPECT_THAT(exec(Limits::infinity(), -q), Result(-Limits::infinity()));
        EXPECT_THAT(exec(-Limits::infinity(), q), Result(-Limits::infinity()));

        // fdiv(+-q1 +-inf) = +0
        EXPECT_THAT(exec(q, Limits::infinity()), Result(TypeParam{0.0}));
        EXPECT_THAT(exec(-q, -Limits::infinity()), Result(TypeParam{0.0}));

        // fdiv(+-q1 -+inf) = -0
        EXPECT_THAT(exec(-q, Limits::infinity()), Result(-TypeParam{0.0}));
        EXPECT_THAT(exec(q, -Limits::infinity()), Result(-TypeParam{0.0}));

        // fdiv(+-0, +-q2) = +0
        EXPECT_THAT(exec(TypeParam{0.0}, q), Result(TypeParam{0.0}));
        EXPECT_THAT(exec(-TypeParam{0.0}, -q), Result(TypeParam{0.0}));

        // fdiv(+-0, -+q2) = -0
        EXPECT_THAT(exec(-TypeParam{0.0}, q), Result(-TypeParam{0.0}));
        EXPECT_THAT(exec(TypeParam{0.0}, -q), Result(-TypeParam{0.0}));

        // fdiv(+-q1, +-0) = +inf
        EXPECT_THAT(exec(q, TypeParam{0.0}), Result(Limits::infinity()));
        EXPECT_THAT(exec(-q, -TypeParam{0.0}), Result(Limits::infinity()));

        // fdiv(+-q1, -+0) = -inf
        EXPECT_THAT(exec(q, -TypeParam{0.0}), Result(-Limits::infinity()));
        EXPECT_THAT(exec(-q, TypeParam{0.0}), Result(-Limits::infinity()));
    }

    EXPECT_THAT(exec(TypeParam{0xABCD.01p7}, TypeParam{4}), Result(TypeParam{0x1.579A02p20}));
}

TYPED_TEST(execute_floating_point_types, min)
{
    using Limits = typename FP<TypeParam>::Limits;

    auto instance = instantiate(parse(this->get_binop_code(Instr::f32_min)));
    const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        const auto& ordered_values = TestValues<TypeParam>::ordered();
        for (const auto z : ordered_values)
        {
            // fmin(+inf, z2) = z2
            EXPECT_THAT(exec(Limits::infinity(), z), Result(z));

            // fmin(-inf, z2) = -inf
            EXPECT_THAT(exec(-Limits::infinity(), z), Result(-Limits::infinity()));

            // fmin(z1, +inf) = z1
            EXPECT_THAT(exec(z, Limits::infinity()), Result(z));

            // fmin(z1, -inf) = -inf
            EXPECT_THAT(exec(z, -Limits::infinity()), Result(-Limits::infinity()));
        }

        // fmin(+-0, -+0) = -0
        EXPECT_THAT(exec(TypeParam{0}, -TypeParam{0}), Result(-TypeParam{0}));
        EXPECT_THAT(exec(-TypeParam{0}, TypeParam{0}), Result(-TypeParam{0}));
        EXPECT_THAT(exec(-TypeParam{0}, -TypeParam{0}), Result(-TypeParam{0}));

        // Check every pair from cartesian product of the list of values.
        // fmin(z1, z2) = z1  (if z1 <= z2)
        // fmin(z1, z2) = z2  (if z2 <= z1)
        for (size_t i = 0; i < std::size(ordered_values); ++i)
        {
            for (size_t j = 0; j < std::size(ordered_values); ++j)
            {
                const auto a = ordered_values[i];
                const auto b = ordered_values[j];
                EXPECT_THAT(exec(a, b), Result(i < j ? a : b)) << a << ", " << b;
            }
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}

TYPED_TEST(execute_floating_point_types, max)
{
    using Limits = typename FP<TypeParam>::Limits;

    auto instance = instantiate(parse(this->get_binop_code(Instr::f32_max)));
    const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };
    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        const auto& ordered_values = TestValues<TypeParam>::ordered();
        for (const auto z : ordered_values)
        {
            // fmax(+inf, z2) = +inf
            EXPECT_THAT(exec(Limits::infinity(), z), Result(Limits::infinity()));

            // fmax(-inf, z2) = z2
            EXPECT_THAT(exec(-Limits::infinity(), z), Result(z));

            // fmax(z1, +inf) = +inf
            EXPECT_THAT(exec(z, Limits::infinity()), Result(Limits::infinity()));

            // fmax(z1, -inf) = z1
            EXPECT_THAT(exec(z, -Limits::infinity()), Result(z));
        }

        // fmax(+-0, -+0) = +0
        EXPECT_THAT(execute(*instance, 0, {TypeParam{0}, -TypeParam{0}}), Result(TypeParam{0}));
        EXPECT_THAT(execute(*instance, 0, {-TypeParam{0}, TypeParam{0}}), Result(TypeParam{0}));
        EXPECT_THAT(execute(*instance, 0, {-TypeParam{0}, -TypeParam{0}}), Result(-TypeParam{0}));

        // Check every pair from cartesian product of the list of values.
        // fmax(z1, z2) = z1  (if z1 >= z2)
        // fmax(z1, z2) = z2  (if z2 >= z1)
        for (size_t i = 0; i < std::size(ordered_values); ++i)
        {
            for (size_t j = 0; j < std::size(ordered_values); ++j)
            {
                const auto a = ordered_values[i];
                const auto b = ordered_values[j];
                EXPECT_THAT(execute(*instance, 0, {a, b}), Result(i > j ? a : b)) << a << ", " << b;
            }
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}

TYPED_TEST(execute_floating_point_types, copysign)
{
    auto instance = instantiate(parse(this->get_binop_code(Instr::f32_copysign)));
    const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };
    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        for (const auto p1 : TestValues<TypeParam>::positive_all())
        {
            for (const auto p2 : TestValues<TypeParam>::positive_all())
            {
                // fcopysign(+-p1, +-p2) = +-p1
                EXPECT_THAT(exec(p1, p2), Result(p1));
                EXPECT_THAT(exec(-p1, -p2), Result(-p1));

                // fcopysign(+-p1, -+p2) = -+p1
                EXPECT_THAT(exec(p1, -p2), Result(-p1));
                EXPECT_THAT(exec(-p1, p2), Result(p1));
            }
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}


TEST(execute_floating_point, f32_load)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result f32)
      (f32.load (local.get 0))
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017d030201000504010101010a0901070020002a02000b");

    auto instance = instantiate(parse(wasm));

    const std::tuple<bytes, float> test_cases[]{
        {"00000000"_bytes, 0.0f},
        {"00000080"_bytes, -0.0f},
        {"b6f39d3f"_bytes, 1.234f},
        {"b6f39dbf"_bytes, -1.234f},
        {"0000807f"_bytes, FP32::Limits::infinity()},
        {"000080ff"_bytes, -FP32::Limits::infinity()},
        {"ffff7f7f"_bytes, FP32::Limits::max()},
        {"ffff7fff"_bytes, -FP32::Limits::max()},
        {"00008000"_bytes, FP32::Limits::min()},
        {"00008080"_bytes, -FP32::Limits::min()},
        {"01000000"_bytes, FP32::Limits::denorm_min()},
        {"01000080"_bytes, -FP32::Limits::denorm_min()},
        {"0000803f"_bytes, 1.0f},
        {"000080bf"_bytes, -1.0f},
        {"ffff7f3f"_bytes, std::nextafter(1.0f, 0.0f)},
        {"ffff7fbf"_bytes, std::nextafter(-1.0f, 0.0f)},
        {"0000c07f"_bytes, FP32::nan(FP32::canon)},
        {"0000c0ff"_bytes, -FP32::nan(FP32::canon)},
        {"0100c07f"_bytes, FP32::nan(FP32::canon + 1)},
        {"0100c0ff"_bytes, -FP32::nan(FP32::canon + 1)},
        {"0100807f"_bytes, FP32::nan(1)},
        {"010080ff"_bytes, -FP32::nan(1)},
    };

    uint32_t address = 0;
    for (const auto& [memory_fill, expected] : test_cases)
    {
        std::copy(memory_fill.begin(), memory_fill.end(), instance->memory->data() + address);

        EXPECT_THAT(execute(*instance, 0, {address}), Result(expected));
        address += static_cast<uint32_t>(memory_fill.size());
    }

    EXPECT_THAT(execute(*instance, 0, {65534}), Traps());
    EXPECT_THAT(execute(*instance, 0, {65537}), Traps());
}

TEST(execute_floating_point, f32_load_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result f32)
      local.get 0
      f32.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017d030201000504010101010a0d010b0020002a02ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0_u32}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute_floating_point, f64_load)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result f64)
      (f64.load (local.get 0))
    )
    */
    const auto wasm =
        from_hex("0061736d0100000001060160017f017c030201000504010101010a0901070020002b03000b");

    auto instance = instantiate(parse(wasm));

    const std::tuple<bytes, double> test_cases[]{
        {"0000000000000000"_bytes, 0.0},
        {"0000000000000080"_bytes, -0.0},
        {"5839b4c876bef33f"_bytes, 1.234},
        {"5839b4c876bef3bf"_bytes, -1.234},
        {"000000000000f07f"_bytes, FP64::Limits::infinity()},
        {"000000000000f0ff"_bytes, -FP64::Limits::infinity()},
        {"ffffffffffffef7f"_bytes, FP64::Limits::max()},
        {"ffffffffffffefff"_bytes, -FP64::Limits::max()},
        {"0000000000001000"_bytes, FP64::Limits::min()},
        {"0000000000001080"_bytes, -FP64::Limits::min()},
        {"0100000000000000"_bytes, FP64::Limits::denorm_min()},
        {"0100000000000080"_bytes, -FP64::Limits::denorm_min()},
        {"000000000000f03f"_bytes, 1.0},
        {"000000000000f0bf"_bytes, -1.0},
        {"ffffffffffffef3f"_bytes, std::nextafter(1.0, 0.0)},
        {"ffffffffffffefbf"_bytes, std::nextafter(-1.0, 0.0)},
        {"000000000000f87f"_bytes, FP64::nan(FP64::canon)},
        {"000000000000f8ff"_bytes, -FP64::nan(FP64::canon)},
        {"010000000000f87f"_bytes, FP64::nan(FP64::canon + 1)},
        {"010000000000f8ff"_bytes, -FP64::nan(FP64::canon + 1)},
        {"010000000000f07f"_bytes, FP64::nan(1)},
        {"010000000000f0ff"_bytes, -FP64::nan(1)},
    };

    uint32_t address = 0;
    for (const auto& [memory_fill, expected] : test_cases)
    {
        std::copy(memory_fill.begin(), memory_fill.end(), instance->memory->data() + address);

        EXPECT_THAT(execute(*instance, 0, {address}), Result(expected));
        address += static_cast<uint32_t>(memory_fill.size());
    }

    EXPECT_THAT(execute(*instance, 0, {65534}), Traps());
    EXPECT_THAT(execute(*instance, 0, {65537}), Traps());
}

TEST(execute_floating_point, f64_load_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32) (result f64)
      local.get 0
      f64.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017c030201000504010101010a0d010b0020002b03ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0_u32}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute_floating_point, f32_store)
{
    /* wat2wasm
    (memory 1 1)
    (data (i32.const 0)  "\cc\cc\cc\cc\cc\cc")
    (func (param f32 i32)
      local.get 1
      local.get 0
      f32.store
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027d7f00030201000504010101010a0b010900200120003802000b0b0c01004100"
        "0b06cccccccccccc");
    const auto module = parse(wasm);

    const std::tuple<float, bytes> test_cases[]
    {
        // clang-format off
        {0.0f, "cc00000000cc"_bytes},
        {-0.0f, "cc00000080cc"_bytes},
        {1.234f, "ccb6f39d3fcc"_bytes},
        {-1.234f, "ccb6f39dbfcc"_bytes},
        {FP32::Limits::infinity(), "cc0000807fcc"_bytes},
        {-FP32::Limits::infinity(), "cc000080ffcc"_bytes},
        {FP32::Limits::max(), "ccffff7f7fcc"_bytes},
        {-FP32::Limits::max(), "ccffff7fffcc"_bytes},
        {FP32::Limits::min(), "cc00008000cc"_bytes},
        {-FP32::Limits::min(), "cc00008080cc"_bytes},
        {FP32::Limits::denorm_min(), "cc01000000cc"_bytes},
        {-FP32::Limits::denorm_min(), "cc01000080cc"_bytes},
        {1.0f, "cc0000803fcc"_bytes},
        {-1.0f, "cc000080bfcc"_bytes},
        {std::nextafter(1.0f, 0.0f), "ccffff7f3fcc"_bytes},
        {std::nextafter(-1.0f, 0.0f), "ccffff7fbfcc"_bytes},
        {FP32::nan(FP32::canon), "cc0000c07fcc"_bytes},
        {-FP32::nan(FP32::canon), "cc0000c0ffcc"_bytes},
        {FP32::nan(FP32::canon + 1), "cc0100c07fcc"_bytes},
        {-FP32::nan(FP32::canon + 1), "cc0100c0ffcc"_bytes},
#if SNAN_SUPPORTED
        {FP32::nan(1), "cc0100807fcc"_bytes}, {-FP32::nan(1), "cc010080ffcc"_bytes},
#endif
        //clang-format on
    };

    for (const auto& [arg, expected] : test_cases)
    {
        auto instance = instantiate(*module);

        EXPECT_THAT(execute(*instance, 0, {arg, 1}), Result());
        EXPECT_EQ(instance->memory->substr(0, 6), expected);

        EXPECT_THAT(execute(*instance, 0, {arg, 65534}), Traps());
        EXPECT_THAT(execute(*instance, 0, {arg, 65537}), Traps());
    }
}

TEST(execute_floating_point, f32_store_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32)
      local.get 0
      f32.const 1.234
      f32.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a12011000200043b6f39d3f3802ffffffff070"
        "b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0_u32}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute_floating_point, f64_store)
{
    /* wat2wasm
    (memory 1 1)
    (data (i32.const 0)  "\cc\cc\cc\cc\cc\cc\cc\cc\cc\cc\cc\cc")
    (func (param f64 i32)
      local.get 1
      local.get 0
      f64.store
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027c7f00030201000504010101010a0b010900200120003903000b0b1201004100"
        "0b0ccccccccccccccccccccccccc");
    const auto module = parse(wasm);

    const std::tuple<double, bytes> test_cases[]
    {
        // clang-format off
        {0.0, "cc0000000000000000cc"_bytes},
        {-0.0, "cc0000000000000080cc"_bytes},
        {1.234, "cc5839b4c876bef33fcc"_bytes},
        {-1.234, "cc5839b4c876bef3bfcc"_bytes},
        {FP64::Limits::infinity(), "cc000000000000f07fcc"_bytes},
        {-FP64::Limits::infinity(), "cc000000000000f0ffcc"_bytes},
        {FP64::Limits::max(), "ccffffffffffffef7fcc"_bytes},
        {-FP64::Limits::max(), "ccffffffffffffefffcc"_bytes},
        {FP64::Limits::min(), "cc0000000000001000cc"_bytes},
        {-FP64::Limits::min(), "cc0000000000001080cc"_bytes},
        {FP64::Limits::denorm_min(), "cc0100000000000000cc"_bytes},
        {-FP64::Limits::denorm_min(), "cc0100000000000080cc"_bytes},
        {1.0, "cc000000000000f03fcc"_bytes},
        {-1.0, "cc000000000000f0bfcc"_bytes},
        {std::nextafter(1.0, 0.0), "ccffffffffffffef3fcc"_bytes},
        {std::nextafter(-1.0, 0.0), "ccffffffffffffefbfcc"_bytes},
        {FP64::nan(FP64::canon), "cc000000000000f87fcc"_bytes},
        {-FP64::nan(FP64::canon), "cc000000000000f8ffcc"_bytes},
        {FP64::nan(FP64::canon + 1), "cc010000000000f87fcc"_bytes},
        {-FP64::nan(FP64::canon + 1), "cc010000000000f8ffcc"_bytes},
#if SNAN_SUPPORTED
        {FP64::nan(1), "cc010000000000f07fcc"_bytes},
        {-FP64::nan(1), "cc010000000000f0ffcc"_bytes},
#endif
        // clang-format on
    };

    for (const auto& [arg, expected] : test_cases)
    {
        auto instance = instantiate(*module);

        EXPECT_THAT(execute(*instance, 0, {arg, 1}), Result());
        EXPECT_EQ(instance->memory->substr(0, 10), expected);

        EXPECT_THAT(execute(*instance, 0, {arg, 65534}), Traps());
        EXPECT_THAT(execute(*instance, 0, {arg, 65537}), Traps());
    }
}

TEST(execute_floating_point, f64_store_overflow)
{
    /* wat2wasm
    (memory 1 1)
    (func (param i32)
      local.get 0
      f64.const 1.234
      f64.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a160114002000445839b4c876bef33f3903ffff"
        "ffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0_u32}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}

TEST(execute_floating_point, f64_add_round_to_even)
{
    // This test presents how IEEE-754 "round to nearest, ties to even" works.
    // This rounding mode is required by WebAssembly.

    struct TestCase
    {
        double a;
        double b;
        double expected_sum;
    };

    constexpr TestCase test_cases[]{
        // = - no rounding, ^ - round up, v - round down.
        {0x1p0, 0x1.0000000000000p0, /* 0x2.0000000000000p0 = */ 0x1.0000000000000p1},
        {0x1p0, 0x1.0000000000001p0, /* 0x2.0000000000001p0 v */ 0x1.0000000000000p1},
        {0x1p0, 0x1.0000000000002p0, /* 0x2.0000000000002p0 = */ 0x1.0000000000001p1},
        {0x1p0, 0x1.0000000000003p0, /* 0x2.0000000000003p0 ^ */ 0x1.0000000000002p1},
        {0x1p0, 0x1.0000000000004p0, /* 0x2.0000000000004p0 = */ 0x1.0000000000002p1},
        {0x1p0, 0x1.0000000000005p0, /* 0x2.0000000000005p0 v */ 0x1.0000000000002p1},
        {0x1p0, 0x1.0000000000006p0, /* 0x2.0000000000006p0 = */ 0x1.0000000000003p1},
        {0x1p0, 0x1.0000000000007p0, /* 0x2.0000000000007p0 ^ */ 0x1.0000000000004p1},
        {0x1p0, 0x1.0000000000008p0, /* 0x2.0000000000008p0 = */ 0x1.0000000000004p1},
        {0x1p0, 0x1.0000000000009p0, /* 0x2.0000000000009p0 v */ 0x1.0000000000004p1},
    };

    /* wat2wasm
    (func (param f64 f64) (result f64)
      (f64.add (local.get 0) (local.get 1))
    )
    */
    const auto wasm = from_hex("0061736d0100000001070160027c7c017c030201000a0901070020002001a00b");

    auto instance = instantiate(parse(wasm));

    for (const auto t : test_cases)
    {
        const auto sum = t.a + t.b;
        ASSERT_EQ(sum, t.expected_sum) << std::hexfloat << t.a << " + " << t.b << " = " << sum
                                       << " (expected: " << t.expected_sum << ")";

        EXPECT_THAT(execute(*instance, 0, {t.a, t.b}), Result(t.expected_sum));
    }
}

TEST(execute_floating_point, f64_add_off_by_one)
{
    // The i386 x87 FPU performs all arithmetic with 80-bit extended precision by default
    // and in the end rounds it to the required type. This causes issues for f64 types
    // because results may be different than when doing computation with 64-bit precision.
    // f32 is not affected.
    // This test presents examples of results which are different in such case.
    // The testfloat tool can easily produce more such cases.
#if defined(__i386__) && !defined(__SSE_MATH__)
    GTEST_SKIP();
#endif

    /* wat2wasm
    (func (param f64 f64) (result f64)
      (f64.add (local.get 0) (local.get 1))
    )
    */
    const auto wasm = from_hex("0061736d0100000001070160027c7c017c030201000a0901070020002001a00b");

    auto instance = instantiate(parse(wasm));

    constexpr auto a = 0x1.0008000008000p60;
    constexpr auto b = 0x1.0000000081fffp07;
    constexpr auto expected = 0x1.0008000008001p60;

    // The precision on the x87 FPU can be changed to 64-bit.
    // See http://christian-seiler.de/projekte/fpmath/.
    /*
    fpu_control_t old_fpu_cw;
    _FPU_GETCW(old_fpu_cw);
    const auto fpu_cw =
        static_cast<fpu_control_t>((old_fpu_cw & ~_FPU_EXTENDED & ~_FPU_SINGLE) | _FPU_DOUBLE);
    _FPU_SETCW(fpu_cw);
    */

    EXPECT_EQ(a + b, expected);  // Check host CPU.
    EXPECT_THAT(execute(*instance, 0, {a, b}), Result(expected));

    /*
    _FPU_SETCW(old_fpu_cw);
    */
}

TEST(execute_floating_point, f64_sub_off_by_one)
{
    // Same as f64_add_off_by_one, but for f64.sub.
#if defined(__i386__) && !defined(__SSE_MATH__)
    GTEST_SKIP();
#endif

    /* wat2wasm
    (func (param f64 f64) (result f64)
      (f64.sub (local.get 0) (local.get 1))
    )
    */
    const auto wasm = from_hex("0061736d0100000001070160027c7c017c030201000a0901070020002001a10b");

    auto instance = instantiate(parse(wasm));

    constexpr auto a = -0x1.ffc3fffffffffp-50;
    constexpr auto b = -0x1.00000000047ffp+04;
    constexpr auto expected = 0x1.00000000047ffp+04;

    EXPECT_EQ(a - b, expected);  // Check host CPU.
    EXPECT_THAT(execute(*instance, 0, {a, b}), Result(expected));
}
