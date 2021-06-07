// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "instructions.hpp"
#include "types.hpp"
#include <gmock/gmock.h>
#include <test/utils/floating_point_utils.hpp>
#include <test/utils/hex.hpp>
#include <array>
#include <cfenv>

// Enables access and modification of the floating-point environment.
// Here we use it to change rounding direction in tests.
// Although required by the C standard, neither GCC nor Clang supports it.
#pragma STDC FENV_ACCESS ON

MATCHER_P(CanonicalNaN, value, "result with a canonical NaN")
{
    (void)value;
    if (arg.trapped || !arg.has_value)
        return false;

    const auto result_value = arg.value.template as<value_type>();
    return fizzy::test::FP<value_type>{result_value}.is_canonical_nan();
}

MATCHER_P(ArithmeticNaN, value, "result with an arithmetic NaN")
{
    (void)value;
    if (arg.trapped || !arg.has_value)
        return false;

    const auto result_value = arg.value.template as<value_type>();
    return fizzy::test::FP<value_type>{result_value}.is_arithmetic_nan();
}

namespace fizzy::test
{
constexpr auto all_rounding_directions = {FE_TONEAREST, FE_DOWNWARD, FE_UPWARD, FE_TOWARDZERO};

template <typename T>
class TestValues
{
    using Limits = typename FP<T>::Limits;

    inline static const FP<T> m_values[]{
        T{0.0},

        Limits::denorm_min(),
        std::nextafter(Limits::denorm_min(), Limits::infinity()),
        std::nextafter(Limits::min(), T{0}),
        Limits::min(),
        std::nextafter(Limits::min(), Limits::infinity()),
        std::nextafter(T{1.0}, T{0}),
        T{1.0},
        std::nextafter(T{1.0}, Limits::infinity()),
        std::nextafter(Limits::max(), T{0}),
        Limits::max(),

        Limits::infinity(),

        // Canonical NaN:
        FP<T>::nan(FP<T>::canon),

        // Arithmetic NaNs:
        FP<T>::nan((FP<T>::canon << 1) - 1),             // All bits set.
        FP<T>::nan(FP<T>::canon | (FP<T>::canon >> 1)),  // Two top bits set.
        FP<T>::nan(FP<T>::canon + 1),

        // Signaling (not arithmetic) NaNs:
        FP<T>::nan(FP<T>::canon >> 1),  // "Standard" signaling NaN.
        FP<T>::nan(2),
        FP<T>::nan(1),
    };

public:
    using Iterator = const FP<T>*;

    static constexpr auto num_nans = 7;
    static constexpr auto num_positive = std::size(m_values) - num_nans;

    static constexpr Iterator first_non_zero = &m_values[1];
    static constexpr Iterator canonical_nan = &m_values[num_positive];
    static constexpr Iterator first_noncanonical_nan = canonical_nan + 1;
    static constexpr Iterator infinity = &m_values[num_positive - 1];

    class Range
    {
        Iterator m_begin;
        Iterator m_end;

    public:
        constexpr Range(Iterator begin, Iterator end) noexcept : m_begin{begin}, m_end{end} {}

        constexpr Iterator begin() const { return m_begin; }
        constexpr Iterator end() const { return m_end; }

        [[gnu::no_sanitize("pointer-subtract")]] constexpr size_t size() const
        {
            // The "pointer-subtract" sanitizer is disabled because GCC fails to compile
            // constexpr function with pointer subtraction.
            // The bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=97145
            // is to be fixed in GCC 10.3.
            return static_cast<size_t>(m_end - m_begin);
        }
    };

    // The list of positive floating-point values without zero, infinity and NaNs.
    static constexpr Range positive_nonzero_finite() noexcept { return {first_non_zero, infinity}; }

    // The list of positive floating-point values without zero and NaNs (includes infinity).
    static constexpr Range positive_nonzero_infinite() noexcept
    {
        return {first_non_zero, canonical_nan};
    }

    // The list of positive NaN values.
    static constexpr Range positive_nans() noexcept { return {canonical_nan, std::end(m_values)}; }

    // The list of positive non-canonical NaN values (including signaling NaNs).
    static constexpr Range positive_noncanonical_nans() noexcept
    {
        return {first_noncanonical_nan, std::end(m_values)};
    }

    // The list of positive floating-point values with zero, infinity and NaNs.
    static constexpr Range positive_all() noexcept
    {
        return {std::begin(m_values), std::end(m_values)};
    }

    // The list of floating-point values, including infinities.
    // They are strictly ordered (ordered_values[i] < ordered_values[j] for i<j).
    // Therefore -0 is omitted. This allows determining the relation of any pair of values only
    // knowing values' position in the array.
    static auto& ordered() noexcept
    {
        static const auto ordered_values = [] {
            constexpr auto ps = positive_nonzero_infinite();
            std::array<FP<T>, ps.size() * 2 + 1> a;

            auto it = std::begin(a);
            it = std::transform(std::make_reverse_iterator(std::end(ps)),
                std::make_reverse_iterator(std::begin(ps)), it, std::negate<FP<T>>{});
            *it++ = FP{T{0.0}};
            std::copy(std::begin(ps), std::end(ps), it);
            return a;
        }();

        return ordered_values;
    }

    // The list of floating-point values, including infinities and NaNs.
    // They are strictly ordered (ordered_values[i] < ordered_values[j] for i<j) or NaNs.
    // Therefore -0 is omitted. This allows determining the relation of any pair of values only
    // knowing values' position in the array.
    static auto& ordered_and_nans() noexcept
    {
        static const auto ordered_values = [] {
            const auto& without_nans = ordered();
            const auto nans = positive_nans();
            std::array<FP<T>, positive_all().size() * 2 - 1> a;

            auto it = std::begin(a);
            it = std::copy(std::begin(without_nans), std::end(without_nans), it);
            it = std::copy(std::begin(nans), std::end(nans), it);
            std::transform(std::begin(nans), std::end(nans), it, std::negate<FP<T>>{});
            return a;
        }();

        return ordered_values;
    }
};

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

    // The [int_only_begin; int_only_end) is the range of floating-point numbers, where each
    // representable number is an integer and there are no fractional numbers between them.
    // These numbers are represented as mantissa [0x1.00..0; 0x1.ff..f]
    // and exponent 2**(mantissa digits without implicit leading 1).
    // The main point of using int_only_begin is to tests nearest() as for int_only_begin - 0.5 and
    // int_only_begin - 1.5 we have equal distance to nearby integer values.
    // (Integer shift is used instead of std::pow() because of the clang compiler bug).
    static constexpr auto int_only_begin = T{uint64_t{1} << (L::digits - 1)};
    static constexpr auto int_only_end = T{uint64_t{1} << L::digits};

    // The list of rounding test cases as pairs (input, expected_trunc) with only positive inputs.
    inline static const std::pair<T, T> positive_trunc_tests[] = {

        // Checks the following rule common for all rounding instructions:
        // f(+-0) = +-0
        {0, 0},

        {L::denorm_min(), 0},
        {L::min(), 0},

        {T{0.1f}, T{0}},

        {std::nextafter(T{0.5}, T{0}), T{0}},
        {T{0.5}, T{0}},
        {std::nextafter(T{0.5}, T{1}), T{0}},

        {std::nextafter(T{1}, T{0}), T{0}},
        {T{1}, T{1}},
        {std::nextafter(T{1}, T{2}), T{1}},

        {std::nextafter(T{1.5}, T{1}), T{1}},
        {T{1.5}, T{1}},
        {std::nextafter(T{1.5}, T{2}), T{1}},

        {std::nextafter(T{2}, T{1}), T{1}},
        {T{2}, T{2}},
        {std::nextafter(T{2}, T{3}), T{2}},

        {int_only_begin - T{2}, int_only_begin - T{2}},
        {int_only_begin - T{1.5}, int_only_begin - T{2}},
        {int_only_begin - T{1}, int_only_begin - T{1}},
        {int_only_begin - T{0.5}, int_only_begin - T{1}},
        {int_only_begin, int_only_begin},
        {int_only_begin + T{1}, int_only_begin + T{1}},

        {int_only_end - T{1}, int_only_end - T{1}},
        {int_only_end, int_only_end},
        {int_only_end + T{2}, int_only_end + T{2}},

        {L::max(), L::max()},

        // Checks the following rule common for all rounding instructions:
        // f(+-inf) = +-inf
        {L::infinity(), L::infinity()},
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
}  // namespace fizzy::test
