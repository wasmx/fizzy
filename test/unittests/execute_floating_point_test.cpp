// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include "execute.hpp"
#include "parser.hpp"
#include "trunc_boundaries.hpp"
#include <gtest/gtest.h>
#include <test/utils/asserts.hpp>
#include <test/utils/floating_point_utils.hpp>
#include <test/utils/hex.hpp>
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

TEST(execute_floating_point, f32_add)
{
    /* wat2wasm
    (func (param f32 f32) (result f32)
      local.get 0
      local.get 1
      f32.add
    )
    */
    const auto wasm = from_hex("0061736d0100000001070160027d7d017d030201000a0901070020002001920b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {1.001f, 6.006f}), Result(7.007f));
}

TEST(execute_floating_point, f64_add)
{
    /* wat2wasm
    (func (param f64 f64) (result f64)
      local.get 0
      local.get 1
      f64.add
    )
    */
    const auto wasm = from_hex("0061736d0100000001070160027c7c017c030201000a0901070020002001a00b");

    auto instance = instantiate(parse(wasm));
    EXPECT_THAT(execute(*instance, 0, {1.0011, 6.0066}), Result(7.0077));
}

template <typename T>
class execute_floating_point_types : public testing::Test
{
};

/// Compile-time information about a Wasm type.
template <typename T>
struct WasmTypeTraits;

template <>
struct WasmTypeTraits<float>
{
    static constexpr auto name = "f32";
};
template <>
struct WasmTypeTraits<double>
{
    static constexpr auto name = "f64";
};

struct WasmTypeName
{
    template <typename T>
    static std::string GetName(int)
    {
        return WasmTypeTraits<T>::name;
    }
};

using FloatingPointTypes = testing::Types<float, double>;
TYPED_TEST_SUITE(execute_floating_point_types, FloatingPointTypes, WasmTypeName);

TYPED_TEST(execute_floating_point_types, compare)
{
    using Limits = typename FP<TypeParam>::Limits;

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

    // The values to be used as test cases.
    // They must be strictly ordered (ordered_values[i] < ordered_values[j] for i<j) or NaNs.
    // This allows determining the relation of any pair of values only knowing values' position
    // in the array.
    const TypeParam ordered_values[] = {
        -Limits::infinity(),
        -Limits::max(),
        std::nextafter(-Limits::max(), TypeParam{0}),
        std::nextafter(-TypeParam{1.0}, -Limits::infinity()),
        -TypeParam{1.0},
        std::nextafter(-TypeParam{1.0}, TypeParam{0}),
        std::nextafter(-Limits::min(), -Limits::infinity()),
        -Limits::min(),
        std::nextafter(-Limits::min(), TypeParam{0}),
        std::nextafter(-Limits::denorm_min(), -Limits::infinity()),
        -Limits::denorm_min(),
        TypeParam{0},
        Limits::denorm_min(),
        std::nextafter(Limits::denorm_min(), Limits::infinity()),
        std::nextafter(Limits::min(), TypeParam{0}),
        Limits::min(),
        std::nextafter(Limits::min(), Limits::infinity()),
        std::nextafter(TypeParam{1.0}, TypeParam{0}),
        TypeParam{1.0},
        std::nextafter(TypeParam{1.0}, Limits::infinity()),
        std::nextafter(Limits::max(), TypeParam{0}),
        Limits::max(),
        Limits::infinity(),

        // NaNs.
        FP<TypeParam>::nan(FP<TypeParam>::canon),
        FP<TypeParam>::nan(FP<TypeParam>::canon + 1),
        FP<TypeParam>::nan(1),
    };

    // Check every pair from cartesian product of ordered_values.
    for (size_t i = 0; i < std::size(ordered_values); ++i)
    {
        for (size_t j = 0; j < std::size(ordered_values); ++j)
        {
            const auto a = ordered_values[i];
            const auto b = ordered_values[j];
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

    // Find and replace changeable values: types and the conversion instruction.
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
