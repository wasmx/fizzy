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
#include <cmath>

using namespace fizzy;
using namespace fizzy::test;

MATCHER_P(CanonicalNaN, value, "result with a canonical NaN")
{
    (void)value;
    if (arg.trapped || !arg.has_value)
        return false;

    const auto result_value = arg.value.template as<value_type>();
    return FP<value_type>{result_value}.nan_payload() == FP<value_type>::canon;
}

MATCHER_P(ArithmeticNaN, value, "result with an arithmetic NaN")
{
    (void)value;
    if (arg.trapped || !arg.has_value)
        return false;

    const auto result_value = arg.value.template as<value_type>();
    return FP<value_type>{result_value}.nan_payload() >= FP<value_type>::canon;
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
protected:
    using L = typename FP<T>::Limits;

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
        FP<T>::nan(FP<T>::canon + 1),
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
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon + 1)}}, Not(CanonicalNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(1)}}, Not(CanonicalNaN(TypeParam{})));

    EXPECT_THAT(Void, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(Trap, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{TypeParam{}}}, Not(ArithmeticNaN(TypeParam{})));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon)}}, ArithmeticNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(FP::canon + 1)}}, ArithmeticNaN(TypeParam{}));
    EXPECT_THAT(ExecutionResult{Value{FP::nan(1)}}, Not(ArithmeticNaN(TypeParam{})));
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
        const auto anan = FP<TypeParam>::nan(FP<TypeParam>::canon + 1);

        EXPECT_THAT(execute(*instance, 0, {q, cnan}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {cnan, q}), CanonicalNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {cnan, cnan}), CanonicalNaN(TypeParam{}));

        EXPECT_THAT(execute(*instance, 0, {q, anan}), ArithmeticNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {anan, q}), ArithmeticNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {anan, anan}), ArithmeticNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {anan, cnan}), ArithmeticNaN(TypeParam{}));
        EXPECT_THAT(execute(*instance, 0, {cnan, anan}), ArithmeticNaN(TypeParam{}));
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


template <typename T>
struct rounding_test_cases
{
    using Limits = typename FP<T>::Limits;

    // The "int only" is the range of the floating-point type of only consecutive integer values.
    inline static const auto int_only_begin = std::pow(T{2}, T{Limits::digits - 1});
    inline static const auto int_only_end = std::pow(T{2}, T{Limits::digits});

    // The list of rounding test cases as pairs (input, expected_trunc) with only positive inputs.
    inline static const std::pair<T, T> tests[]{
        //        {0, 0},
        //        {Limits::denorm_min(), 0},
        //        {Limits::min(), 0},
        //        {std::nextafter(T{1}, T{0}), T{0}},
        //        {T{1}, T{1}},
        //        {std::nextafter(T{1}, T{2}), T{1}},
        //        {std::nextafter(T{2}, T{1}), T{1}},
        //        {T{2}, T{2}},
        //        {int_only_begin - T{1}, int_only_begin - T{1}},
        {int_only_begin - T{0.5}, int_only_begin - T{1}},
        //        {int_only_begin, int_only_begin},
        //        {int_only_begin + T{1}, int_only_begin + T{1}},
        //        {int_only_end - T{1}, int_only_end - T{1}},
        //        {int_only_end, int_only_end},
        //        {int_only_end + T{2}, int_only_end + T{2}},
        //        {Limits::max(), Limits::max()},
        //        {Limits::infinity(), Limits::infinity()},
    };
};

TYPED_TEST(execute_floating_point_types, ceil)
{
    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_ceil)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    for (const auto& [arg, expected_trunc] : rounding_test_cases<TypeParam>::tests)
    {
        // For positive values, the ceil() is trunc() + 1, unless the input is already an integer.
        const auto expected_pos =
            (arg == expected_trunc) ? expected_trunc : expected_trunc + TypeParam{1};
        EXPECT_THAT(exec(arg), Result(expected_pos)) << arg << ": " << expected_pos;

        // For negative values, the ceil() is trunc().
        EXPECT_THAT(exec(-arg), Result(-expected_trunc)) << -arg << ": " << -expected_trunc;
    }
}

TYPED_TEST(execute_floating_point_types, floor)
{
    auto instance = instantiate(parse(this->get_unop_code(Instr::f32_floor)));
    const auto exec = [&](auto arg) { return execute(*instance, 0, {arg}); };

    for (const auto& [arg, expected_trunc] : rounding_test_cases<TypeParam>::tests)
    {
        // For positive values, the floor() is trunc().
        EXPECT_THAT(exec(arg), Result(expected_trunc)) << arg << ": " << expected_trunc;

        // For negative values, the floor() is trunc() - 1, unless the input is already an integer.
        const auto expected_neg =
            (arg == expected_trunc) ? -expected_trunc : -expected_trunc - TypeParam{1};
        EXPECT_THAT(exec(-arg), Result(expected_neg)) << -arg << ": " << expected_neg;
    }
}

TEST(clang, bug)
{
    EXPECT_NE(rounding_test_cases<double>::tests[0].first, -0.5);
}
