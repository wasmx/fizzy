// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "instructions.hpp"
#include "parser.hpp"
#include "trunc_boundaries.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/floating_point_utils.hpp>
#include <test/utils/hex.hpp>
#include <array>
#include <cfenv>
#include <cmath>

#pragma STDC FENV_ACCESS ON

using namespace fizzy;
using namespace fizzy::test;

MATCHER_P(CanonicalNaN, value, "result with a canonical NaN")
{
    (void)value;
    if (arg.trapped || !arg.has_value)
        return false;

    const auto result_value = arg.value.template as<value_type>();
    return FP<value_type>{result_value}.is_canonical_nan();
}

MATCHER_P(ArithmeticNaN, value, "result with an arithmetic NaN")
{
    (void)value;
    if (arg.trapped || !arg.has_value)
        return false;

    const auto result_value = arg.value.template as<value_type>();
    return FP<value_type>{result_value}.is_arithmetic_nan();
}

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

/// Compile-time information about a Wasm type.
template <typename T>
struct WasmTypeTraits;

template <>
struct WasmTypeTraits<float>
{
    static constexpr auto name = "f32";
    static constexpr auto valtype = ValType::f32;
};
template <>
struct WasmTypeTraits<double>
{
    static constexpr auto name = "f64";
    static constexpr auto valtype = ValType::f64;
};

struct WasmTypeName
{
    template <typename T>
    static std::string GetName(int)
    {
        return WasmTypeTraits<T>::name;
    }
};

template <typename T>
class execute_floating_point_types : public testing::Test
{
public:
    using L = typename FP<T>::Limits;

    static constexpr auto all_rounding_directions = {
        FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};

    // The list of positive floating-point values without zeros, infinities and NaNs.
    inline static const std::array positive_special_values{
        L::denorm_min(),
        L::min(),
        std::nextafter(T{1.0}, T{0.0}),
        T{1.0},
        std::nextafter(T{1.0}, L::infinity()),
        L::max(),
    };

    // The list of floating-point values, including infinities and NaNs.
    // They must be strictly ordered (ordered_values[i] < ordered_values[j] for i<j) or NaNs.
    // Therefore -0 is omitted. This allows determining the relation of any pair of values only
    // knowing values' position in the array.
    inline static const std::array ordered_special_values = {
        -L::infinity(),
        -L::max(),
        std::nextafter(-L::max(), T{0}),
        std::nextafter(-T{1.0}, -L::infinity()),
        -T{1.0},
        std::nextafter(-T{1.0}, T{0}),
        std::nextafter(-L::min(), -L::infinity()),
        -L::min(),
        std::nextafter(-L::min(), T{0}),
        std::nextafter(-L::denorm_min(), -L::infinity()),
        -L::denorm_min(),
        T{0},
        L::denorm_min(),
        std::nextafter(L::denorm_min(), L::infinity()),
        std::nextafter(L::min(), T{0}),
        L::min(),
        std::nextafter(L::min(), L::infinity()),
        std::nextafter(T{1.0}, T{0}),
        T{1.0},
        std::nextafter(T{1.0}, L::infinity()),
        std::nextafter(L::max(), T{0}),
        L::max(),
        L::infinity(),

        // NaNs.
        FP<T>::nan(FP<T>::canon),
        -FP<T>::nan(FP<T>::canon),
        FP<T>::nan(FP<T>::canon + 1),
        -FP<T>::nan(FP<T>::canon + 1),
        FP<T>::nan(1),
        -FP<T>::nan(1),
    };

    inline static const std::array positive_noncanonical_nans = {
        // Arithmetic:
        FP<T>::nan((FP<T>::canon << 1) - 1),             // All bits set.
        FP<T>::nan(FP<T>::canon | (FP<T>::canon >> 1)),  // Two top bits set.
        FP<T>::nan(FP<T>::canon + 1),

        // Signaling (not arithmetic):
        FP<T>::nan(FP<T>::canon >> 1),  // "Standard" signaling NaN.
        FP<T>::nan(2),
        FP<T>::nan(1),
    };

    /// Creates a wasm module with a single function for the given instructions opcode.
    /// The opcode is converted to match the type, e.g. f32_add -> f64_add.
    static bytes get_numeric_instruction_code(
        bytes_view template_code, Instr template_opcode, Instr opcode)
    {
        constexpr auto f64_variant_offset =
            static_cast<uint8_t>(Instr::f64_add) - static_cast<uint8_t>(Instr::f32_add);

        // Convert to f64 variant if needed.
        const auto typed_opcode =
            std::is_same_v<T, double> ?
                static_cast<uint8_t>(static_cast<uint8_t>(opcode) + f64_variant_offset) :
                static_cast<uint8_t>(opcode);

        bytes wasm{template_code};
        constexpr auto template_type = static_cast<uint8_t>(ValType::f32);
        const auto template_opcode_byte = static_cast<uint8_t>(template_opcode);
        const auto opcode_arity = get_instruction_type_table()[template_opcode_byte].inputs.size();

        EXPECT_EQ(std::count(wasm.begin(), wasm.end(), template_type), opcode_arity + 1);
        EXPECT_EQ(std::count(wasm.begin(), wasm.end(), template_opcode_byte), 1);

        std::replace(wasm.begin(), wasm.end(), template_type,
            static_cast<uint8_t>(WasmTypeTraits<T>::valtype));
        std::replace(wasm.begin(), wasm.end(), template_opcode_byte, typed_opcode);
        return wasm;
    }

    static bytes get_unop_code(Instr opcode)
    {
        /* wat2wasm
        (func (param f32) (result f32)
          (f32.abs (local.get 0))
        )
        */
        auto wasm = from_hex("0061736d0100000001060160017d017d030201000a0701050020008b0b");
        return get_numeric_instruction_code(wasm, Instr::f32_abs, opcode);
    }

    static bytes get_binop_code(Instr opcode)
    {
        /* wat2wasm
        (func (param f32 f32) (result f32)
          (f32.add (local.get 0) (local.get 1))
        )
        */
        auto wasm = from_hex("0061736d0100000001070160027d7d017d030201000a0901070020002001920b");
        return get_numeric_instruction_code(wasm, Instr::f32_add, opcode);
    }
};

using FloatingPointTypes = testing::Types<float, double>;
TYPED_TEST_SUITE(execute_floating_point_types, FloatingPointTypes, WasmTypeName);

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
    EXPECT_THAT(ExecutionResult{Value{FP::nan(1)}}, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{-FP::nan(1)}}, Not(ArithmeticNaN(TypeParam{})));
}

TYPED_TEST(execute_floating_point_types, unop_nan_propagation)
{
    // Tests NaN propagation in unary instructions (unop).
    // If NaN input is canonical NN, the result must be the canonical NaN.
    // Otherwise, the result must be an arithmetic NaN.

    // The list of instructions to be tested.
    // Only f32 variants, but f64 variants are going to be covered as well.
    constexpr Instr opcodes[] = {
        Instr::f32_sqrt,
    };

    for (const auto op : opcodes)
    {
        auto instance = instantiate(parse(this->get_unop_code(op)));

        const auto cnan = FP<TypeParam>::nan(FP<TypeParam>::canon);
        EXPECT_THAT(execute(*instance, 0, {cnan}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {-cnan}), CanonicalNaN(TypeParam{}));

        for (const auto nan : this->positive_noncanonical_nans)
        {
            const auto res1 = execute(*instance, 0, {nan});
            EXPECT_THAT(res1, ArithmeticNaN(TypeParam{}))
                << std::hex << FP<TypeParam>{res1.value.template as<TypeParam>()}.nan_payload();
            const auto res2 = execute(*instance, 0, {-nan});
            EXPECT_THAT(res2, ArithmeticNaN(TypeParam{}))
                << std::hex << FP<TypeParam>{res2.value.template as<TypeParam>()}.nan_payload();
        }
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

        constexpr auto q = TypeParam{1.0};
        const auto cnan = FP<TypeParam>::nan(FP<TypeParam>::canon);

        // TODO: Consider more restrictive tests where the sign of NaN values is also checked.

        EXPECT_THAT(execute(*instance, 0, {q, cnan}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {q, -cnan}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {cnan, q}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {-cnan, q}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {cnan, cnan}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {cnan, -cnan}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {-cnan, cnan}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {-cnan, -cnan}), CanonicalNaN(TypeParam{}));

        for (const auto nan : this->positive_noncanonical_nans)
        {
            EXPECT_THAT(execute(*instance, 0, {q, nan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {q, -nan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {nan, q}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {-nan, q}), ArithmeticNaN(TypeParam{}));

            EXPECT_THAT(execute(*instance, 0, {nan, nan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {nan, -nan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {-nan, nan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {-nan, -nan}), ArithmeticNaN(TypeParam{}));

            EXPECT_THAT(execute(*instance, 0, {nan, cnan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {nan, -cnan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {-nan, cnan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {-nan, -cnan}), ArithmeticNaN(TypeParam{}));

            EXPECT_THAT(execute(*instance, 0, {cnan, nan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {cnan, -nan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {-cnan, nan}), ArithmeticNaN(TypeParam{}));
            EXPECT_THAT(execute(*instance, 0, {-cnan, -nan}), ArithmeticNaN(TypeParam{}));
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
    auto inst = instantiate(parse(wasm));

    constexpr FuncIdx func_offset = std::is_same_v<TypeParam, float> ? 0 : 6;
    constexpr auto eq = func_offset + 0;
    constexpr auto ne = func_offset + 1;
    constexpr auto lt = func_offset + 2;
    constexpr auto gt = func_offset + 3;
    constexpr auto le = func_offset + 4;
    constexpr auto ge = func_offset + 5;

    // Check every pair from cartesian product of ordered_values.
    for (size_t i = 0; i < std::size(this->ordered_special_values); ++i)
    {
        for (size_t j = 0; j < std::size(this->ordered_special_values); ++j)
        {
            const auto a = this->ordered_special_values[i];
            const auto b = this->ordered_special_values[j];
            if (std::isnan(a) || std::isnan(b))
            {
                EXPECT_THAT(execute(*inst, eq, {a, b}), Result(0)) << a << "==" << b;
                EXPECT_THAT(execute(*inst, ne, {a, b}), Result(1)) << a << "!=" << b;
                EXPECT_THAT(execute(*inst, lt, {a, b}), Result(0)) << a << "<" << b;
                EXPECT_THAT(execute(*inst, gt, {a, b}), Result(0)) << a << ">" << b;
                EXPECT_THAT(execute(*inst, le, {a, b}), Result(0)) << a << "<=" << b;
                EXPECT_THAT(execute(*inst, ge, {a, b}), Result(0)) << a << ">=" << b;
            }
            else
            {
                EXPECT_THAT(execute(*inst, eq, {a, b}), Result(uint32_t{i == j})) << a << "==" << b;
                EXPECT_THAT(execute(*inst, ne, {a, b}), Result(uint32_t{i != j})) << a << "!=" << b;
                EXPECT_THAT(execute(*inst, lt, {a, b}), Result(uint32_t{i < j})) << a << "<" << b;
                EXPECT_THAT(execute(*inst, gt, {a, b}), Result(uint32_t{i > j})) << a << ">" << b;
                EXPECT_THAT(execute(*inst, le, {a, b}), Result(uint32_t{i <= j})) << a << "<=" << b;
                EXPECT_THAT(execute(*inst, ge, {a, b}), Result(uint32_t{i >= j})) << a << ">=" << b;
            }
        }
    }

    // Negative zero. This is separate set of checks because -0.0 cannot be placed
    // in the ordered_values array as -0.0 == 0.0.
    EXPECT_THAT(execute(*inst, eq, {TypeParam{-0.0}, TypeParam{0.0}}), Result(1));
    EXPECT_THAT(execute(*inst, ne, {TypeParam{-0.0}, TypeParam{0.0}}), Result(0));
    EXPECT_THAT(execute(*inst, lt, {TypeParam{-0.0}, TypeParam{0.0}}), Result(0));
    EXPECT_THAT(execute(*inst, gt, {TypeParam{-0.0}, TypeParam{0.0}}), Result(0));
    EXPECT_THAT(execute(*inst, le, {TypeParam{-0.0}, TypeParam{0.0}}), Result(1));
    EXPECT_THAT(execute(*inst, ge, {TypeParam{-0.0}, TypeParam{0.0}}), Result(1));
}

TYPED_TEST(execute_floating_point_types, abs)
{
    using FP = FP<TypeParam>;
    using Limits = typename FP::Limits;

    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_abs)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    std::vector p_values(
        std::begin(this->positive_special_values), std::end(this->positive_special_values));
    for (const auto x :
        {TypeParam{0}, Limits::infinity(), FP::nan(FP::canon), FP::nan(FP::canon + 1), FP::nan(1)})
        p_values.push_back(x);

    for (const auto p : p_values)
    {
        // fabs(+-p) = +p
        EXPECT_THAT(exec(p), Result(p));
        EXPECT_THAT(exec(-p), Result(p));
    }
}

TYPED_TEST(execute_floating_point_types, neg)
{
    using FP = FP<TypeParam>;
    using Limits = typename FP::Limits;

    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_neg)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    std::vector p_values(
        std::begin(this->positive_special_values), std::end(this->positive_special_values));
    for (const auto x :
        {TypeParam{0}, Limits::infinity(), FP::nan(FP::canon), FP::nan(FP::canon + 1), FP::nan(1)})
        p_values.push_back(x);

    for (const auto p : p_values)
    {
        // fneg(+-p) = -+p
        EXPECT_THAT(exec(p), Result(-p));
        EXPECT_THAT(exec(-p), Result(p));
    }
}

TYPED_TEST(execute_floating_point_types, sqrt)
{
    using FP = FP<TypeParam>;
    using Limits = typename FP::Limits;

    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_sqrt)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : this->all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        // fsqrt(-inf) = nan:canonical
        EXPECT_THAT(exec(-Limits::infinity()), CanonicalNaN(TypeParam{}));

        // fsqrt(+inf) = +inf
        EXPECT_THAT(exec(Limits::infinity()), Result(Limits::infinity()));

        // fsqrt(+-0) = +-0
        EXPECT_THAT(exec(TypeParam{0.0}), Result(TypeParam{0.0}));
        EXPECT_THAT(exec(-TypeParam{0.0}), Result(-TypeParam{0.0}));

        for (const auto p : this->positive_special_values)
        {
            // fsqrt(-p) = nan:canonical
            EXPECT_THAT(exec(-p), CanonicalNaN(TypeParam{}));
        }

        EXPECT_THAT(exec(TypeParam{1}), Result(TypeParam{1}));
        EXPECT_THAT(exec(TypeParam{4}), Result(TypeParam{2}));
        EXPECT_THAT(exec(TypeParam{0x1.21p6}), Result(TypeParam{0x1.1p3}));  // exact
        EXPECT_THAT(exec(std::nextafter(TypeParam{0x1.21p6}, TypeParam{0})),
            Result(TypeParam{0x1.1p3}));  // rounded up
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
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

    for (const auto q : this->positive_special_values)
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

    for (const auto q : this->positive_special_values)
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

    for (const auto z1 : this->ordered_special_values)
    {
        for (const auto z2 : this->ordered_special_values)
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

    for (const auto q : this->positive_special_values)
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

    for (const auto q : this->positive_special_values)
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

    for (const auto z : this->ordered_special_values)
    {
        if (std::isnan(z))
            continue;

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
    for (size_t i = 0; i < std::size(this->ordered_special_values); ++i)
    {
        for (size_t j = 0; j < std::size(this->ordered_special_values); ++j)
        {
            const auto a = this->ordered_special_values[i];
            const auto b = this->ordered_special_values[j];
            if (!std::isnan(a) && !std::isnan(b))
            {
                EXPECT_THAT(exec(a, b), Result(i < j ? a : b)) << a << ", " << b;
            }
        }
    }
}

TYPED_TEST(execute_floating_point_types, max)
{
    using Limits = typename FP<TypeParam>::Limits;

    auto instance = instantiate(parse(this->get_binop_code(Instr::f32_max)));
    const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };

    for (const auto z : this->ordered_special_values)
    {
        if (std::isnan(z))
            continue;

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
    for (size_t i = 0; i < std::size(this->ordered_special_values); ++i)
    {
        for (size_t j = 0; j < std::size(this->ordered_special_values); ++j)
        {
            const auto a = this->ordered_special_values[i];
            const auto b = this->ordered_special_values[j];
            if (!std::isnan(a) && !std::isnan(b))
            {
                EXPECT_THAT(execute(*instance, 0, {a, b}), Result(i > j ? a : b)) << a << ", " << b;
            }
        }
    }
}

TYPED_TEST(execute_floating_point_types, copysign)
{
    using FP = FP<TypeParam>;
    using Limits = typename FP::Limits;

    auto instance = instantiate(parse(this->get_binop_code(Instr::f32_copysign)));
    const auto exec = [&](auto arg1, auto arg2) { return execute(*instance, 0, {arg1, arg2}); };

    std::vector p_values(
        std::begin(this->positive_special_values), std::end(this->positive_special_values));
    for (const auto x :
        {TypeParam{0}, Limits::infinity(), FP::nan(FP::canon), FP::nan(FP::canon + 1), FP::nan(1)})
        p_values.push_back(x);

    for (const auto p1 : p_values)
    {
        for (const auto p2 : p_values)
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


TEST(execute_floating_point, f64_promote_f32)
{
    /* wat2wasm
    (func (param f32) (result f64)
      local.get 0
      f64.promote_f32
    )
    */
    const auto wasm = from_hex("0061736d0100000001060160017d017c030201000a070105002000bb0b");
    auto instance = instantiate(parse(wasm));

    const std::pair<float, double> test_cases[] = {
        {0.0f, 0.0},
        {-0.0f, -0.0},
        {1.0f, 1.0},
        {-1.0f, -1.0},
        {FP32::Limits::lowest(), double{FP32::Limits::lowest()}},
        {FP32::Limits::max(), double{FP32::Limits::max()}},
        {FP32::Limits::min(), double{FP32::Limits::min()}},
        {FP32::Limits::denorm_min(), double{FP32::Limits::denorm_min()}},
        {FP32::Limits::infinity(), FP64::Limits::infinity()},
        {-FP32::Limits::infinity(), -FP64::Limits::infinity()},

        // The canonical NaN must result in canonical NaN (only the top bit set).
        {FP32::nan(FP32::canon), FP64::nan(FP64::canon)},
        {-FP32::nan(FP32::canon), -FP64::nan(FP64::canon)},
    };

    for (const auto& [arg, expected] : test_cases)
    {
        EXPECT_THAT(execute(*instance, 0, {arg}), Result(expected)) << arg << " -> " << expected;
    }

    // Check arithmetic NaNs (payload >= canonical payload).
    // The following check expect arithmetic NaNs. Canonical NaNs are arithmetic NaNs
    // and are allowed by the spec in these situations, but our checks are more restrictive

    // An arithmetic NaN must result in any arithmetic NaN.
    const auto res1 = execute(*instance, 0, {FP32::nan(FP32::canon + 1)});
    ASSERT_TRUE(!res1.trapped && res1.has_value);
    EXPECT_EQ(std::signbit(res1.value.f64), 0);
    EXPECT_GT(FP{res1.value.f64}.nan_payload(), FP64::canon);
    const auto res2 = execute(*instance, 0, {-FP32::nan(FP32::canon + 1)});
    ASSERT_TRUE(!res2.trapped && res2.has_value);
    EXPECT_EQ(std::signbit(res2.value.f64), 1);
    EXPECT_GT(FP{res2.value.f64}.nan_payload(), FP64::canon);

    // Other NaN must also result in arithmetic NaN.
    const auto res3 = execute(*instance, 0, {FP32::nan(1)});
    ASSERT_TRUE(!res3.trapped && res3.has_value);
    EXPECT_EQ(std::signbit(res3.value.f64), 0);
    EXPECT_GT(FP{res3.value.f64}.nan_payload(), FP64::canon);
    const auto res4 = execute(*instance, 0, {-FP32::nan(1)});
    ASSERT_TRUE(!res4.trapped && res4.has_value);
    EXPECT_EQ(std::signbit(res4.value.f64), 1);
    EXPECT_GT(FP{res4.value.f64}.nan_payload(), FP64::canon);

    // Any input NaN other than canonical must result in an arithmetic NaN.
    for (const auto nan : execute_floating_point_types<float>::positive_noncanonical_nans)
    {
        EXPECT_THAT(execute(*instance, 0, {nan}), ArithmeticNaN(double{}));
        EXPECT_THAT(execute(*instance, 0, {-nan}), ArithmeticNaN(double{}));
    }
}

TEST(execute_floating_point, f32_demote_f64)
{
    /* wat2wasm
    (func (param f64) (result f32)
      local.get 0
      f32.demote_f64
    )
    */
    const auto wasm = from_hex("0061736d0100000001060160017c017d030201000a070105002000b60b");
    auto instance = instantiate(parse(wasm));

    constexpr double f32_max = FP32::Limits::max();
    ASSERT_EQ(f32_max, 0x1.fffffep127);

    // The "artificial" f32 range limit: the next f32 number that could be represented
    // if the exponent had a larger range.
    // Wasm spec Rounding section denotes this as the limit_N in the float_N function (for N=32).
    // https://webassembly.github.io/spec/core/exec/numerics.html#rounding
    constexpr double f32_limit = 0x1p128;  // 2**128.

    // The lower boundary input value that results in the infinity. The number is midway between
    // f32_max and f32_limit. For this value rounding prefers infinity, because f32_limit is even.
    constexpr double lowest_to_inf = (f32_max + f32_limit) / 2;
    ASSERT_EQ(lowest_to_inf, 0x1.ffffffp127);

    const std::pair<double, float> test_cases[] = {
        // demote(+-0) = +-0
        {0.0, 0.0f},
        {-0.0, -0.0f},

        {1.0, 1.0f},
        {-1.0, -1.0f},
        {double{FP32::Limits::lowest()}, FP32::Limits::lowest()},
        {double{FP32::Limits::max()}, FP32::Limits::max()},
        {double{FP32::Limits::min()}, FP32::Limits::min()},
        {double{-FP32::Limits::min()}, -FP32::Limits::min()},
        {double{FP32::Limits::denorm_min()}, FP32::Limits::denorm_min()},
        {double{-FP32::Limits::denorm_min()}, -FP32::Limits::denorm_min()},

        // Some special f64 values.
        {FP64::Limits::lowest(), -FP32::Limits::infinity()},
        {FP64::Limits::max(), FP32::Limits::infinity()},
        {FP64::Limits::min(), 0.0f},
        {-FP64::Limits::min(), -0.0f},
        {FP64::Limits::denorm_min(), 0.0f},
        {-FP64::Limits::denorm_min(), -0.0f},

        // Out of range values rounded to max/lowest.
        {std::nextafter(f32_max, FP64::Limits::infinity()), FP32::Limits::max()},
        {std::nextafter(double{FP32::Limits::lowest()}, -FP64::Limits::infinity()),
            FP32::Limits::lowest()},

        {std::nextafter(lowest_to_inf, 0.0), FP32::Limits::max()},
        {std::nextafter(-lowest_to_inf, 0.0), FP32::Limits::lowest()},

        // The smallest of range values rounded to infinity.
        {lowest_to_inf, FP32::Limits::infinity()},
        {-lowest_to_inf, -FP32::Limits::infinity()},

        {std::nextafter(lowest_to_inf, FP64::Limits::infinity()), FP32::Limits::infinity()},
        {std::nextafter(-lowest_to_inf, -FP64::Limits::infinity()), -FP32::Limits::infinity()},

        // float_32(r) = +inf  (if r >= +limit_32)
        {f32_limit, FP32::Limits::infinity()},

        // float_32(r) = -inf  (if r <= -limit_32)
        {-f32_limit, -FP32::Limits::infinity()},

        // demote(+-inf) = +-inf
        {FP64::Limits::infinity(), FP32::Limits::infinity()},
        {-FP64::Limits::infinity(), -FP32::Limits::infinity()},

        // Rounding.
        {0x1.fffffefffffffp0, 0x1.fffffep0f},  // round down
        {0x1.fffffe0000000p0, 0x1.fffffep0f},  // exact (odd)
        {0x1.fffffd0000001p0, 0x1.fffffep0f},  // round up

        {0x1.fffff8p0, 0x1.fffff8p0f},                       // exact (even)
        {(0x1.fffff8p0 + 0x1.fffffap0) / 2, 0x1.fffff8p0f},  // tie-to-even down
        {0x1.fffffap0, 0x1.fffffap0f},                       // exact (odd)
        {(0x1.fffffap0 + 0x1.fffffcp0) / 2, 0x1.fffffcp0f},  // tie-to-even up
        {0x1.fffffcp0, 0x1.fffffcp0f},                       // exact (even)

        // The canonical NaN must result in canonical NaN (only the top bit of payload set).
        {FP32::nan(FP32::canon), FP64::nan(FP64::canon)},
        {-FP32::nan(FP32::canon), -FP64::nan(FP64::canon)},
    };

    for (const auto& [arg, expected] : test_cases)
    {
        EXPECT_THAT(execute(*instance, 0, {arg}), Result(expected)) << arg << " -> " << expected;
    }

    // Any input NaN other than canonical must result in an arithmetic NaN.
    for (const auto nan : execute_floating_point_types<double>::positive_noncanonical_nans)
    {
        EXPECT_THAT(execute(*instance, 0, {nan}), ArithmeticNaN(float{}));
        EXPECT_THAT(execute(*instance, 0, {-nan}), ArithmeticNaN(float{}));
    }
}

TYPED_TEST(execute_floating_point_types, reinterpret)
{
    /* wat2wasm
    (func (param f32) (result i32) (i32.reinterpret_f32 (local.get 0)))
    (func (param f64) (result i64) (i64.reinterpret_f64 (local.get 0)))
    (func (param i32) (result f32) (f32.reinterpret_i32 (local.get 0)))
    (func (param i64) (result f64) (f64.reinterpret_i64 (local.get 0)))
    */
    const auto wasm = from_hex(
        "0061736d0100000001150460017d017f60017c017e60017f017d60017e017c030504000102030a190405002000"
        "bc0b05002000bd0b05002000be0b05002000bf0b");
    auto instance = instantiate(parse(wasm));
    const auto func_float_to_int = std::is_same_v<TypeParam, float> ? 0 : 1;
    const auto func_int_to_float = std::is_same_v<TypeParam, float> ? 2 : 3;

    for (const auto float_value : this->ordered_special_values)
    {
        const auto uint_value = FP<TypeParam>{float_value}.as_uint();
        EXPECT_THAT(execute(*instance, func_float_to_int, {float_value}), Result(uint_value));
        EXPECT_THAT(execute(*instance, func_int_to_float, {uint_value}), Result(float_value));
    }
}


template <typename SrcT, typename DstT>
struct ConversionPairWasmTraits;

template <>
struct ConversionPairWasmTraits<float, int32_t>
{
    static constexpr auto opcode_name = "i32_trunc_f32_s";
    static constexpr auto opcode = Instr::i32_trunc_f32_s;
    static constexpr auto src_valtype = ValType::f32;
    static constexpr auto dst_valtype = ValType::i32;
};
template <>
struct ConversionPairWasmTraits<float, uint32_t>
{
    static constexpr auto opcode_name = "i32_trunc_f32_u";
    static constexpr auto opcode = Instr::i32_trunc_f32_u;
    static constexpr auto src_valtype = ValType::f32;
    static constexpr auto dst_valtype = ValType::i32;
};
template <>
struct ConversionPairWasmTraits<double, int32_t>
{
    static constexpr auto opcode_name = "i32_trunc_f64_s";
    static constexpr auto opcode = Instr::i32_trunc_f64_s;
    static constexpr auto src_valtype = ValType::f64;
    static constexpr auto dst_valtype = ValType::i32;
};
template <>
struct ConversionPairWasmTraits<double, uint32_t>
{
    static constexpr auto opcode_name = "i32_trunc_f64_u";
    static constexpr auto opcode = Instr::i32_trunc_f64_u;
    static constexpr auto src_valtype = ValType::f64;
    static constexpr auto dst_valtype = ValType::i32;
};
template <>
struct ConversionPairWasmTraits<float, int64_t>
{
    static constexpr auto opcode_name = "i64_trunc_f32_s";
    static constexpr auto opcode = Instr::i64_trunc_f32_s;
    static constexpr auto src_valtype = ValType::f32;
    static constexpr auto dst_valtype = ValType::i64;
};
template <>
struct ConversionPairWasmTraits<float, uint64_t>
{
    static constexpr auto opcode_name = "i64_trunc_f32_u";
    static constexpr auto opcode = Instr::i64_trunc_f32_u;
    static constexpr auto src_valtype = ValType::f32;
    static constexpr auto dst_valtype = ValType::i64;
};
template <>
struct ConversionPairWasmTraits<double, int64_t>
{
    static constexpr auto opcode_name = "i64_trunc_f64_s";
    static constexpr auto opcode = Instr::i64_trunc_f64_s;
    static constexpr auto src_valtype = ValType::f64;
    static constexpr auto dst_valtype = ValType::i64;
};
template <>
struct ConversionPairWasmTraits<double, uint64_t>
{
    static constexpr auto opcode_name = "i64_trunc_f64_u";
    static constexpr auto opcode = Instr::i64_trunc_f64_u;
    static constexpr auto src_valtype = ValType::f64;
    static constexpr auto dst_valtype = ValType::i64;
};
template <>
struct ConversionPairWasmTraits<int32_t, float>
{
    static constexpr auto opcode_name = "f32_convert_i32_s";
    static constexpr auto opcode = Instr::f32_convert_i32_s;
    static constexpr auto src_valtype = ValType::i32;
    static constexpr auto dst_valtype = ValType::f32;
};
template <>
struct ConversionPairWasmTraits<uint32_t, float>
{
    static constexpr auto opcode_name = "f32_convert_i32_u";
    static constexpr auto opcode = Instr::f32_convert_i32_u;
    static constexpr auto src_valtype = ValType::i32;
    static constexpr auto dst_valtype = ValType::f32;
};
template <>
struct ConversionPairWasmTraits<int64_t, float>
{
    static constexpr auto opcode_name = "f32_convert_i64_s";
    static constexpr auto opcode = Instr::f32_convert_i64_s;
    static constexpr auto src_valtype = ValType::i64;
    static constexpr auto dst_valtype = ValType::f32;
};
template <>
struct ConversionPairWasmTraits<uint64_t, float>
{
    static constexpr auto opcode_name = "f32_convert_i64_u";
    static constexpr auto opcode = Instr::f32_convert_i64_u;
    static constexpr auto src_valtype = ValType::i64;
    static constexpr auto dst_valtype = ValType::f32;
};
template <>
struct ConversionPairWasmTraits<int32_t, double>
{
    static constexpr auto opcode_name = "f64_convert_i32_s";
    static constexpr auto opcode = Instr::f64_convert_i32_s;
    static constexpr auto src_valtype = ValType::i32;
    static constexpr auto dst_valtype = ValType::f64;
};
template <>
struct ConversionPairWasmTraits<uint32_t, double>
{
    static constexpr auto opcode_name = "f64_convert_i32_u";
    static constexpr auto opcode = Instr::f64_convert_i32_u;
    static constexpr auto src_valtype = ValType::i32;
    static constexpr auto dst_valtype = ValType::f64;
};
template <>
struct ConversionPairWasmTraits<int64_t, double>
{
    static constexpr auto opcode_name = "f64_convert_i64_s";
    static constexpr auto opcode = Instr::f64_convert_i64_s;
    static constexpr auto src_valtype = ValType::i64;
    static constexpr auto dst_valtype = ValType::f64;
};
template <>
struct ConversionPairWasmTraits<uint64_t, double>
{
    static constexpr auto opcode_name = "f64_convert_i64_u";
    static constexpr auto opcode = Instr::f64_convert_i64_u;
    static constexpr auto src_valtype = ValType::i64;
    static constexpr auto dst_valtype = ValType::f64;
};

template <typename SrcT, typename DstT>
struct ConversionPair : ConversionPairWasmTraits<SrcT, DstT>
{
    using src_type = SrcT;
    using dst_type = DstT;
};

struct ConversionName
{
    template <typename T>
    static std::string GetName(int /*unused*/)
    {
        return T::opcode_name;
    }
};

template <typename T>
class execute_floating_point_trunc : public testing::Test
{
};

using TruncPairs = testing::Types<ConversionPair<float, int32_t>, ConversionPair<float, uint32_t>,
    ConversionPair<double, int32_t>, ConversionPair<double, uint32_t>,
    ConversionPair<float, int64_t>, ConversionPair<float, uint64_t>,
    ConversionPair<double, int64_t>, ConversionPair<double, uint64_t>>;
TYPED_TEST_SUITE(execute_floating_point_trunc, TruncPairs, ConversionName);

TYPED_TEST(execute_floating_point_trunc, trunc)
{
    using FloatT = typename TypeParam::src_type;
    using IntT = typename TypeParam::dst_type;
    using FloatLimits = std::numeric_limits<FloatT>;
    using IntLimits = std::numeric_limits<IntT>;

    /* wat2wasm
    (func (param f32) (result i32)
      local.get 0
      i32.trunc_f32_s
    )
    */
    auto wasm = from_hex("0061736d0100000001060160017d017f030201000a070105002000a80b");

    // Find and replace changeable values: types and the trunc instruction.
    constexpr auto param_type = static_cast<uint8_t>(ValType::f32);
    constexpr auto result_type = static_cast<uint8_t>(ValType::i32);
    constexpr auto opcode = static_cast<uint8_t>(Instr::i32_trunc_f32_s);
    ASSERT_EQ(std::count(wasm.begin(), wasm.end(), param_type), 1);
    ASSERT_EQ(std::count(wasm.begin(), wasm.end(), result_type), 1);
    ASSERT_EQ(std::count(wasm.begin(), wasm.end(), opcode), 1);
    *std::find(wasm.begin(), wasm.end(), param_type) = static_cast<uint8_t>(TypeParam::src_valtype);
    *std::find(wasm.begin(), wasm.end(), result_type) =
        static_cast<uint8_t>(TypeParam::dst_valtype);
    *std::find(wasm.begin(), wasm.end(), opcode) = static_cast<uint8_t>(TypeParam::opcode);

    auto instance = instantiate(parse(wasm));

    // Zero.
    EXPECT_THAT(execute(*instance, 0, {FloatT{0}}), Result(IntT{0}));
    EXPECT_THAT(execute(*instance, 0, {-FloatT{0}}), Result(IntT{0}));

    // Something around 0.0.
    EXPECT_THAT(execute(*instance, 0, {FloatLimits::denorm_min()}), Result(IntT{0}));
    EXPECT_THAT(execute(*instance, 0, {-FloatLimits::denorm_min()}), Result(IntT{0}));

    // Something smaller than 2.0.
    EXPECT_THAT(execute(*instance, 0, {std::nextafter(FloatT{2}, FloatT{0})}), Result(IntT{1}));

    // Something bigger than -1.0.
    EXPECT_THAT(execute(*instance, 0, {std::nextafter(FloatT{-1}, FloatT{0})}), Result(IntT{0}));

    {
        // BOUNDARIES OF DEFINITION
        //
        // Here we want to identify and test the boundary values of the defined behavior of the
        // trunc instructions. For undefined results the execution must trap.
        // Note that floating point type can represent any power of 2.

        using expected_boundaries = trunc_boundaries<FloatT, IntT>;

        // For iN with max value 2^N-1 the float(2^N) exists and trunc(float(2^N)) to iN
        // is undefined.
        const auto upper_boundary = std::pow(FloatT{2}, FloatT{IntLimits::digits});
        EXPECT_EQ(upper_boundary, expected_boundaries::upper);
        EXPECT_THAT(execute(*instance, 0, {upper_boundary}), Traps());

        // But the trunc() of the next float value smaller than 2^N is defined.
        // Depending on the resolution of the floating point type, the result integer value may
        // be other than 2^(N-1).
        const auto max_defined = std::nextafter(upper_boundary, FloatT{0});
        const auto max_defined_int = static_cast<IntT>(max_defined);
        EXPECT_THAT(execute(*instance, 0, {max_defined}), Result(max_defined_int));

        // The lower boundary is:
        // - for signed integers: -2^N - 1,
        // - for unsigned integers: -1.
        // However, the -2^N - 1 may be not representative in a float type so we compute it as
        // floor(-2^N - epsilon).
        const auto min_defined_int = IntLimits::min();
        const auto lower_boundary =
            std::floor(std::nextafter(FloatT{min_defined_int}, -FloatLimits::infinity()));
        EXPECT_EQ(lower_boundary, expected_boundaries::lower);
        EXPECT_THAT(execute(*instance, 0, {lower_boundary}), Traps());

        const auto min_defined = std::nextafter(lower_boundary, FloatT{0});
        EXPECT_THAT(execute(*instance, 0, {min_defined}), Result(min_defined_int));
    }

    {
        // NaNs.
        EXPECT_THAT(execute(*instance, 0, {FloatLimits::quiet_NaN()}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FloatLimits::signaling_NaN()}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FP<FloatT>::nan(FP<FloatT>::canon)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-FP<FloatT>::nan(FP<FloatT>::canon)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FP<FloatT>::nan(FP<FloatT>::canon + 1)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-FP<FloatT>::nan(FP<FloatT>::canon + 1)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FP<FloatT>::nan(1)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-FP<FloatT>::nan(1)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FP<FloatT>::nan(0xdead)}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-FP<FloatT>::nan(0xdead)}), Traps());
        const auto signaling_nan = FP<FloatT>::nan(FP<FloatT>::canon >> 1);
        EXPECT_THAT(execute(*instance, 0, {signaling_nan}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-signaling_nan}), Traps());

        const auto inf = FloatLimits::infinity();
        EXPECT_THAT(execute(*instance, 0, {inf}), Traps());
        EXPECT_THAT(execute(*instance, 0, {-inf}), Traps());

        EXPECT_THAT(execute(*instance, 0, {FloatLimits::max()}), Traps());
        EXPECT_THAT(execute(*instance, 0, {FloatLimits::lowest()}), Traps());
    }

    if constexpr (IntLimits::is_signed)
    {
        // Something bigger than -2.0.
        const auto arg = std::nextafter(FloatT{-2}, FloatT{0});
        const auto result = execute(*instance, 0, {arg});
        EXPECT_EQ(result.value.template as<IntT>(), FloatT{-1});
    }
}


template <typename T>
class execute_floating_point_convert : public testing::Test
{
};

using ConvertPairs = testing::Types<ConversionPair<int32_t, float>, ConversionPair<uint32_t, float>,
    ConversionPair<int64_t, float>, ConversionPair<uint64_t, float>,
    ConversionPair<int32_t, double>, ConversionPair<uint32_t, double>,
    ConversionPair<int64_t, double>, ConversionPair<uint64_t, double>>;
TYPED_TEST_SUITE(execute_floating_point_convert, ConvertPairs, ConversionName);

TYPED_TEST(execute_floating_point_convert, convert)
{
    using IntT = typename TypeParam::src_type;
    using FloatT = typename TypeParam::dst_type;
    using IntLimits = std::numeric_limits<IntT>;
    using FloatLimits = std::numeric_limits<FloatT>;

    /* wat2wasm
    (func (param i32) (result f32)
      local.get 0
      f32.convert_i32_s
    )
    */
    auto wasm = from_hex("0061736d0100000001060160017f017d030201000a070105002000b20b");

    // Find and replace changeable values: types and the convert instruction.
    constexpr auto param_type = static_cast<uint8_t>(ValType::i32);
    constexpr auto result_type = static_cast<uint8_t>(ValType::f32);
    constexpr auto opcode = static_cast<uint8_t>(Instr::f32_convert_i32_s);
    ASSERT_EQ(std::count(wasm.begin(), wasm.end(), param_type), 1);
    ASSERT_EQ(std::count(wasm.begin(), wasm.end(), result_type), 1);
    ASSERT_EQ(std::count(wasm.begin(), wasm.end(), opcode), 1);
    *std::find(wasm.begin(), wasm.end(), param_type) = static_cast<uint8_t>(TypeParam::src_valtype);
    *std::find(wasm.begin(), wasm.end(), result_type) =
        static_cast<uint8_t>(TypeParam::dst_valtype);
    *std::find(wasm.begin(), wasm.end(), opcode) = static_cast<uint8_t>(TypeParam::opcode);

    auto instance = instantiate(parse(wasm));

    EXPECT_THAT(execute(*instance, 0, {IntT{0}}), Result(FloatT{0}));
    EXPECT_THAT(execute(*instance, 0, {IntT{1}}), Result(FloatT{1}));

    // Max integer value: 2^N - 1.
    constexpr auto max = IntLimits::max();
    // Can the FloatT represent all values of IntT?
    constexpr auto exact = IntLimits::digits < FloatLimits::digits;
    // For "exact" the result is just 2^N - 1, for "not exact" the nearest to 2^N - 1 is 2^N.
    const auto max_expected = std::pow(FloatT{2}, FloatT{IntLimits::digits}) - FloatT{exact};
    EXPECT_THAT(execute(*instance, 0, {max}), Result(max_expected));

    if constexpr (IntLimits::is_signed)
    {
        EXPECT_THAT(execute(*instance, 0, {-IntT{1}}), Result(-FloatT{1}));

        static_assert(std::is_same_v<decltype(-max), IntT>);
        EXPECT_THAT(execute(*instance, 0, {-max}), Result(-max_expected));

        const auto min_expected = -std::pow(FloatT{2}, FloatT{IntLimits::digits});
        EXPECT_THAT(execute(*instance, 0, {IntLimits::min()}), Result(min_expected));
    }
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
      get_local 0
      f32.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017d030201000504010101010a0d010b0020002a02ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0}), Traps());
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
      get_local 0
      f64.load offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160017f017c030201000504010101010a0d010b0020002b03ffffffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0}), Traps());
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
      get_local 1
      get_local 0
      f32.store
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027d7f00030201000504010101010a0b010900200120003802000b0b0c01004100"
        "0b06cccccccccccc");
    const auto module = parse(wasm);

    const std::tuple<float, bytes> test_cases[]{
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
        {FP32::nan(1), "cc0100807fcc"_bytes},
        {-FP32::nan(1), "cc010080ffcc"_bytes},
    };

    for (const auto& [arg, expected] : test_cases)
    {
        auto instance = instantiate(module);

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
      get_local 0
      f32.const 1.234
      f32.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a12011000200043b6f39d3f3802ffffffff070"
        "b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0}), Traps());
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
      get_local 1
      get_local 0
      f64.store
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001060160027c7f00030201000504010101010a0b010900200120003903000b0b1201004100"
        "0b0ccccccccccccccccccccccccc");
    const auto module = parse(wasm);

    const std::tuple<double, bytes> test_cases[]{
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
        {FP64::nan(1), "cc010000000000f07fcc"_bytes},
        {-FP64::nan(1), "cc010000000000f0ffcc"_bytes},
    };

    for (const auto& [arg, expected] : test_cases)
    {
        auto instance = instantiate(module);

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
      get_local 0
      f64.const 1.234
      f64.store offset=0x7fffffff
    )
    */
    const auto wasm = from_hex(
        "0061736d0100000001050160017f00030201000504010101010a160114002000445839b4c876bef33f3903ffff"
        "ffff070b");

    auto instance = instantiate(parse(wasm));

    // Offset is 0x7fffffff + 0 => 0x7fffffff
    EXPECT_THAT(execute(*instance, 0, {0}), Traps());
    // Offset is 0x7fffffff + 0x80000000 => 0xffffffff
    EXPECT_THAT(execute(*instance, 0, {0x80000000}), Traps());
    // Offset is 0x7fffffff + 0x80000001 => 0x100000000
    EXPECT_THAT(execute(*instance, 0, {0x80000001}), Traps());
}
