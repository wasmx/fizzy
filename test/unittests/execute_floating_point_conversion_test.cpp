// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "execute_floating_point_test.hpp"
#include "instructions.hpp"
#include "parser.hpp"
#include "trunc_boundaries.hpp"
#include <test/utils/asserts.hpp>
#include <test/utils/execute_helpers.hpp>
#include <test/utils/hex.hpp>
#include <cmath>

using namespace fizzy;
using namespace fizzy::test;

TEST(execute_floating_point_conversion, f64_promote_f32)
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

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        for (const auto& [arg, expected] : test_cases)
        {
            EXPECT_THAT(execute(*instance, 0, {arg}), Result(expected))
                << arg << " -> " << expected;
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
        for (const auto nan : TestValues<float>::positive_noncanonical_nans())
        {
            EXPECT_THAT(execute(*instance, 0, {nan}), ArithmeticNaN(double{}));
            EXPECT_THAT(execute(*instance, 0, {-nan}), ArithmeticNaN(double{}));
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
}

TEST(execute_floating_point_conversion, f32_demote_f64)
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
    for (const auto nan : TestValues<double>::positive_noncanonical_nans())
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

    ASSERT_EQ(std::fegetround(), FE_TONEAREST);
    for (const auto rounding_direction : all_rounding_directions)
    {
        ASSERT_EQ(std::fesetround(rounding_direction), 0);
        SCOPED_TRACE(rounding_direction);

        const auto& ordered_values = TestValues<TypeParam>::ordered_and_nans();
        for (const auto float_value : ordered_values)
        {
            const auto uint_value = FP<TypeParam>{float_value}.as_uint();
            EXPECT_THAT(execute(*instance, func_float_to_int, {float_value}), Result(uint_value));
            EXPECT_THAT(execute(*instance, func_int_to_float, {uint_value}), Result(float_value));
        }
    }
    ASSERT_EQ(std::fesetround(FE_TONEAREST), 0);
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
