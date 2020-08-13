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
