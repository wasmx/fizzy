// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/floating_point_utils.hpp>

using namespace fizzy::test;

TEST(floating_point_utils, fp_default)
{
    EXPECT_EQ(FP32{}.as_uint(), 0);
    EXPECT_EQ(FP32{}.as_float(), 0.0f);
    EXPECT_EQ(FP32{}.sign_bit(), 0);

    EXPECT_EQ(FP64{}.as_uint(), 0);
    EXPECT_EQ(FP64{}.as_float(), 0.0);
    EXPECT_EQ(FP64{}.sign_bit(), 0);
}

TEST(floating_point_utils, fp_sign_bit)
{
    EXPECT_EQ(FP{0.0f}.sign_bit(), 0);
    EXPECT_EQ(FP{-0.0f}.sign_bit(), 1);
    EXPECT_EQ(FP(FP32::Limits::infinity()).sign_bit(), 0);
    EXPECT_EQ(FP(-FP32::Limits::infinity()).sign_bit(), 1);
    EXPECT_EQ(FP(FP32::Limits::max()).sign_bit(), 0);
    EXPECT_EQ(FP(FP32::Limits::lowest()).sign_bit(), 1);
    EXPECT_EQ(FP32::nan(FP32::canon).sign_bit(), 0);
    EXPECT_EQ((-FP32::nan(FP32::canon)).sign_bit(), 1);

    EXPECT_EQ(FP{0.0}.sign_bit(), 0);
    EXPECT_EQ(FP{-0.0}.sign_bit(), 1);
    EXPECT_EQ(FP(FP64::Limits::infinity()).sign_bit(), 0);
    EXPECT_EQ(FP(-FP64::Limits::infinity()).sign_bit(), 1);
    EXPECT_EQ(FP(FP64::Limits::max()).sign_bit(), 0);
    EXPECT_EQ(FP(FP64::Limits::lowest()).sign_bit(), 1);
    EXPECT_EQ(FP64::nan(FP64::canon).sign_bit(), 0);
    EXPECT_EQ((-FP64::nan(FP64::canon)).sign_bit(), 1);
}

TEST(floating_point_utils, fp_negate)
{
    EXPECT_EQ((-FP{1.0f}).as_float(), -1.0f);
    EXPECT_EQ((-FP{1.0}).as_float(), -1.0);
    EXPECT_EQ((-FP{-1.0f}).as_float(), 1.0f);
    EXPECT_EQ((-FP{-1.0}).as_float(), 1.0);
}

TEST(floating_point_utils, double_as_uint)
{
    EXPECT_EQ(FP(0.0).as_uint(), 0x0000000000000000);
    EXPECT_EQ(FP(-0.0).as_uint(), 0x8000000000000000);
    EXPECT_EQ(FP(FP64::Limits::infinity()).as_uint(), 0x7FF'0000000000000);
    EXPECT_EQ(FP(-FP64::Limits::infinity()).as_uint(), 0xFFF'0000000000000);
    EXPECT_EQ(FP(FP64::Limits::max()).as_uint(), 0x7FE'FFFFFFFFFFFFF);
    EXPECT_EQ(FP(-FP64::Limits::max()).as_uint(), 0xFFE'FFFFFFFFFFFFF);
    EXPECT_EQ(FP(FP64::Limits::min()).as_uint(), 0x001'0000000000000);
    EXPECT_EQ(FP(-FP64::Limits::min()).as_uint(), 0x801'0000000000000);
    EXPECT_EQ(FP(FP64::Limits::denorm_min()).as_uint(), 0x000'0000000000001);
    EXPECT_EQ(FP(-FP64::Limits::denorm_min()).as_uint(), 0x800'0000000000001);
    EXPECT_EQ(FP(1.0).as_uint(), 0x3FF'0000000000000);
    EXPECT_EQ(FP(-1.0).as_uint(), 0xBFF'0000000000000);
    EXPECT_EQ(FP(std::nextafter(1.0, 0.0)).as_uint(), 0x3FE'FFFFFFFFFFFFF);
    EXPECT_EQ(FP(std::nextafter(-1.0, 0.0)).as_uint(), 0xBFE'FFFFFFFFFFFFF);
    EXPECT_EQ(FP(FP64::nan(FP64::canon)).as_uint(), 0x7FF'8000000000000);
    EXPECT_EQ(FP(-FP64::nan(FP64::canon)).as_uint(), 0xFFF'8000000000000);
}

TEST(floating_point_utils, binary_representation_implementation_defined)
{
    EXPECT_EQ(FP(FP64::Limits::quiet_NaN()).as_uint(), 0x7FF'8000000000000);
    EXPECT_EQ(FP(FP64::Limits::quiet_NaN()).nan_payload(), 0x8000000000000);
    EXPECT_EQ(FP(FP32::Limits::quiet_NaN()).as_uint(), 0x7FC00000);
    EXPECT_EQ(FP(FP32::Limits::quiet_NaN()).nan_payload(), 0x400000);

#if SNAN_SUPPORTED
    EXPECT_EQ(FP(FP64::Limits::signaling_NaN()).as_uint(), 0x7FF'4000000000000);
    EXPECT_EQ(FP(FP64::Limits::signaling_NaN()).nan_payload(), 0x4000000000000);
    EXPECT_EQ(FP(FP32::Limits::signaling_NaN()).as_uint(), 0x7FA00000);
    EXPECT_EQ(FP(FP32::Limits::signaling_NaN()).nan_payload(), 0x200000);
#endif
}

TEST(floating_point_utils, float_as_uint)
{
    EXPECT_EQ(FP(0.0f).as_uint(), 0x00000000);
    EXPECT_EQ(FP(-0.0f).as_uint(), 0x80000000);
    EXPECT_EQ(FP(FP32::Limits::infinity()).as_uint(), 0x7F800000);
    EXPECT_EQ(FP(-FP32::Limits::infinity()).as_uint(), 0xFF800000);
    EXPECT_EQ(FP(FP32::Limits::max()).as_uint(), 0x7F7FFFFF);
    EXPECT_EQ(FP(-FP32::Limits::max()).as_uint(), 0xFF7FFFFF);
    EXPECT_EQ(FP(FP32::Limits::min()).as_uint(), 0x00800000);
    EXPECT_EQ(FP(-FP32::Limits::min()).as_uint(), 0x80800000);
    EXPECT_EQ(FP(FP32::Limits::denorm_min()).as_uint(), 0x00000001);
    EXPECT_EQ(FP(-FP32::Limits::denorm_min()).as_uint(), 0x80000001);
    EXPECT_EQ(FP(1.0f).as_uint(), 0x3F800000);
    EXPECT_EQ(FP(-1.0f).as_uint(), 0xBF800000);
    EXPECT_EQ(FP(std::nextafter(1.0f, 0.0f)).as_uint(), 0x3F7FFFFF);
    EXPECT_EQ(FP(std::nextafter(-1.0f, 0.0f)).as_uint(), 0xBF7FFFFF);
    EXPECT_EQ(FP(FP32::nan(FP32::canon)).as_uint(), 0x7FC00000);
    EXPECT_EQ(FP(-FP32::nan(FP32::canon)).as_uint(), 0xFFC00000);
}

TEST(floating_point_utils, double_from_uint)
{
    EXPECT_EQ(FP(uint64_t{0x0000000000000000}).as_float(), 0.0);
    EXPECT_EQ(FP(uint64_t{0x8000000000000000}).as_float(), -0.0);
    EXPECT_EQ(FP(uint64_t{0x3FF'000000000DEAD}).as_float(), 0x1.000000000DEADp0);
    EXPECT_EQ(FP(uint64_t{0xBFF'000000000DEAD}).as_float(), -0x1.000000000DEADp0);
    EXPECT_EQ(FP(uint64_t{0x7FF'0000000000000}).as_float(), FP64::Limits::infinity());
    EXPECT_EQ(FP(uint64_t{0xFFF'0000000000000}).as_float(), -FP64::Limits::infinity());
}

TEST(floating_point_utils, float_from_uint)
{
    EXPECT_EQ(FP(uint32_t{0x00000000}).as_float(), 0.0f);
    EXPECT_EQ(FP(uint32_t{0x80000000}).as_float(), -0.0f);
    EXPECT_EQ(FP(uint32_t{0x3FEF5680}).as_float(), 0x1.DEADp0f);
    EXPECT_EQ(FP(uint32_t{0xBFEF5680}).as_float(), -0x1.DEADp0f);
    EXPECT_EQ(FP(uint32_t{0x7F800000}).as_float(), FP32::Limits::infinity());
    EXPECT_EQ(FP(uint32_t{0xFF800000}).as_float(), -FP32::Limits::infinity());
}

TEST(floating_point_utils, double_nan_payload)
{
    constexpr auto inf = FP64::Limits::infinity();
    const auto qnan = FP64::nan(FP64::canon);

    EXPECT_EQ(FP(0.0).nan_payload(), 0);
    EXPECT_EQ(FP(FP64::nan(FP64::canon + 1)).nan_payload(), FP64::canon + 1);
    EXPECT_EQ(FP(qnan).nan_payload(), FP64::canon);
    EXPECT_EQ(FP(qnan.as_float() + 1.0).nan_payload(), FP64::canon);
    EXPECT_EQ(FP(inf - inf).nan_payload(), FP64::canon);
    EXPECT_EQ(FP(inf * 0.0).nan_payload(), FP64::canon);

#if SNAN_SUPPORTED
    EXPECT_EQ(FP(FP64::nan(1)).nan_payload(), 1);
#endif
}

TEST(floating_point_utils, float_nan_payload)
{
    constexpr auto inf = FP32::Limits::infinity();
    const auto qnan = FP32::nan(FP32::canon);

    EXPECT_EQ(FP(0.0f).nan_payload(), 0);
    EXPECT_EQ(FP(FP32::nan(1)).nan_payload(), 1);
    EXPECT_EQ(FP(FP32::nan(FP32::canon + 1)).nan_payload(), FP32::canon + 1);
    EXPECT_EQ(FP(qnan).nan_payload(), FP32::canon);
    EXPECT_EQ(FP(qnan.as_float() + 1.0f).nan_payload(), FP32::canon);
    EXPECT_EQ(FP(inf - inf).nan_payload(), FP32::canon);
    EXPECT_EQ(FP(inf * 0.0f).nan_payload(), FP32::canon);

#if SNAN_SUPPORTED
    EXPECT_EQ(FP(FP32::nan(1)).nan_payload(), 1);
#endif
}

TEST(floating_point_utils, double_nan)
{
    EXPECT_TRUE(FP64::nan(FP64::canon).is_nan());
    EXPECT_TRUE(std::isnan(FP64::nan(FP64::canon).as_float()));
    EXPECT_TRUE(FP64::nan(1).is_nan());
    EXPECT_TRUE(std::isnan(FP64::nan(1).as_float()));
    EXPECT_TRUE(FP64::nan(0xDEADBEEF).is_nan());
    EXPECT_TRUE(std::isnan(FP64::nan(0xDEADBEEF).as_float()));
    EXPECT_TRUE(FP64::nan(0xDEADBEEFBEEEF).is_nan());
    EXPECT_TRUE(std::isnan(FP64::nan(0xDEADBEEFBEEEF).as_float()));
    EXPECT_FALSE(FP64::nan(0).is_nan());
    EXPECT_FALSE(std::isnan(FP64::nan(0).as_float()));

    EXPECT_EQ(FP{FP64::nan(FP64::canon)}.nan_payload(), FP64::canon);

    EXPECT_EQ(FP{FP64::nan(FP64::canon)}.as_uint(), 0x7FF'8000000000000);
    EXPECT_EQ(FP{FP64::nan(0xDEADBEEF)}.as_uint(), 0x7FF'00000DEADBEEF);
    EXPECT_EQ(FP{FP64::nan(0xDEADBEEFBEEEF)}.as_uint(), 0x7FF'DEADBEEFBEEEF);
}

TEST(floating_point_utils, float_nan)
{
    EXPECT_TRUE(FP32::nan(FP32::canon).is_nan());
    EXPECT_TRUE(std::isnan(FP32::nan(FP32::canon).as_float()));
    EXPECT_TRUE(FP32::nan(1).is_nan());
    EXPECT_TRUE(std::isnan(FP32::nan(1).as_float()));
    EXPECT_TRUE(FP32::nan(0x7fffff).is_nan());
    EXPECT_TRUE(std::isnan(FP32::nan(0x7fffff).as_float()));
    EXPECT_TRUE(FP32::nan(0x400001).is_nan());
    EXPECT_TRUE(std::isnan(FP32::nan(0x400001).as_float()));
    EXPECT_FALSE(FP32::nan(0).is_nan());
    EXPECT_FALSE(std::isnan(FP32::nan(0).as_float()));

    EXPECT_EQ(FP{FP32::nan(FP32::canon)}.nan_payload(), FP32::canon);

    EXPECT_EQ(FP{FP32::nan(FP32::canon)}.as_uint(), 0x7FC00000);
    EXPECT_EQ(FP{FP32::nan(0x7FFFFF)}.as_uint(), 0x7FFFFFFF);
    EXPECT_EQ(FP{FP32::nan(0x400001)}.as_uint(), 0x7FC00001);
}

TEST(floating_point_utils, std_nan)
{
    EXPECT_EQ(FP(std::nan("")).nan_payload(), FP64::canon);
    EXPECT_EQ(FP(std::nan("1")).nan_payload(), FP64::canon + 1);
    EXPECT_EQ(FP(std::nan("0xDEAD")).nan_payload(), FP64::canon + 0xDEAD);
}

TEST(floating_point_utils, compare_double)
{
    const auto one = 1.0;
    const auto inf = FP64::Limits::infinity();
    const auto cnan = FP64::nan(FP64::canon);
    const auto snan = FP64::nan(1);

    EXPECT_EQ(FP{one}, FP{one});
    EXPECT_EQ(FP{one}, one);
    EXPECT_EQ(one, FP{one});

    EXPECT_EQ(FP{inf}, FP{inf});
    EXPECT_EQ(FP{inf}, inf);
    EXPECT_EQ(inf, FP{inf});

    EXPECT_EQ(FP{cnan}, FP{cnan});
    EXPECT_EQ(FP{cnan}, cnan);
    EXPECT_EQ(cnan, FP{cnan});

    EXPECT_EQ(FP{snan}, FP{snan});
    EXPECT_EQ(FP{snan}, snan);
    EXPECT_EQ(snan, FP{snan});

    EXPECT_NE(FP{one}, FP{inf});
    EXPECT_NE(FP{one}, inf);
    EXPECT_NE(one, FP{inf});

    EXPECT_NE(FP{one}, FP{cnan});
    EXPECT_NE(FP{one}, cnan);
    EXPECT_NE(one, FP{cnan});

    EXPECT_NE(FP{one}, FP{snan});
    EXPECT_NE(FP{one}, snan);
    EXPECT_NE(one, FP{snan});

    EXPECT_NE(FP{inf}, FP{cnan});
    EXPECT_NE(FP{inf}, cnan);
    EXPECT_NE(inf, FP{cnan});

    EXPECT_NE(FP{inf}, FP{snan});
    EXPECT_NE(FP{inf}, snan);
    EXPECT_NE(inf, FP{snan});

    EXPECT_NE(FP{cnan}, FP{snan});
    EXPECT_NE(FP{cnan}, snan);
    EXPECT_NE(cnan, FP{snan});
}

TEST(floating_point_utils, compare_zero)
{
    EXPECT_EQ(FP{0.0}, FP{0.0});
    EXPECT_EQ(FP{-0.0}, FP{-0.0});
    EXPECT_EQ(FP{0.0f}, FP{0.0f});
    EXPECT_EQ(FP{-0.0f}, FP{-0.0f});

    EXPECT_NE(FP{-0.0}, FP{0.0});
    EXPECT_NE(FP{0.0}, FP{-0.0});
    EXPECT_NE(FP{-0.0f}, FP{0.0f});
    EXPECT_NE(FP{0.0f}, FP{-0.0f});
}

TEST(floating_point_utils, double_is_canonical_nan)
{
    // canonical
    EXPECT_TRUE(FP64{FP64::nan(FP64::canon)}.is_canonical_nan());
    EXPECT_TRUE(FP64{-FP64::nan(FP64::canon)}.is_canonical_nan());

    // arithmetic
    EXPECT_FALSE(FP64{FP64::nan(FP64::canon + 1)}.is_canonical_nan());
    EXPECT_FALSE(FP64{-FP64::nan(FP64::canon + 1)}.is_canonical_nan());
    EXPECT_FALSE(FP64{FP64::nan(FP64::canon + 0xDEADBEEF)}.is_canonical_nan());
    EXPECT_FALSE(FP64{-FP64::nan(FP64::canon + 0xDEADBEEF)}.is_canonical_nan());
    EXPECT_FALSE(FP64{FP64::nan(FP64::mantissa_mask)}.is_canonical_nan());
    EXPECT_FALSE(FP64{-FP64::nan(FP64::mantissa_mask)}.is_canonical_nan());

    // non-arithmetic
    EXPECT_FALSE(FP64{FP64::nan(1)}.is_canonical_nan());
    EXPECT_FALSE(FP64{-FP64::nan(1)}.is_canonical_nan());
    EXPECT_FALSE(FP64{FP64::nan(0xDEADBEEF)}.is_canonical_nan());
    EXPECT_FALSE(FP64{-FP64::nan(0xDEADBEEF)}.is_canonical_nan());
    EXPECT_FALSE(FP64{FP64::nan(0x0DEADBEEFBEEF)}.is_canonical_nan());
    EXPECT_FALSE(FP64{-FP64::nan(0x0DEADBEEFBEEF)}.is_canonical_nan());

    // not NaN
    EXPECT_FALSE(FP64{0.0}.is_canonical_nan());
    EXPECT_FALSE(FP64{-0.0}.is_canonical_nan());
    EXPECT_FALSE(FP64{1.234}.is_canonical_nan());
    EXPECT_FALSE(FP64{-1.234}.is_canonical_nan());
    EXPECT_FALSE(FP64{FP64::Limits::infinity()}.is_canonical_nan());
    EXPECT_FALSE(FP64{-FP64::Limits::infinity()}.is_canonical_nan());
}

TEST(floating_point_utils, double_is_arithmetic_nan)
{
    // canonical
    EXPECT_TRUE(FP64{FP64::nan(FP64::canon)}.is_arithmetic_nan());
    EXPECT_TRUE(FP64{-FP64::nan(FP64::canon)}.is_arithmetic_nan());

    // arithmetic
    EXPECT_TRUE(FP64{FP64::nan(FP64::canon + 1)}.is_arithmetic_nan());
    EXPECT_TRUE(FP64{-FP64::nan(FP64::canon + 1)}.is_arithmetic_nan());
    EXPECT_TRUE(FP64{FP64::nan(FP64::canon + 0xDEADBEEF)}.is_arithmetic_nan());
    EXPECT_TRUE(FP64{-FP64::nan(FP64::canon + 0xDEADBEEF)}.is_arithmetic_nan());
    EXPECT_TRUE(FP64{FP64::nan(FP64::mantissa_mask)}.is_arithmetic_nan());
    EXPECT_TRUE(FP64{-FP64::nan(FP64::mantissa_mask)}.is_arithmetic_nan());

#if SNAN_SUPPORTED
    // non-arithmetic
    EXPECT_FALSE(FP64{FP64::nan(1)}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{-FP64::nan(1)}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{FP64::nan(0xDEADBEEF)}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{-FP64::nan(0xDEADBEEF)}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{FP64::nan(0x0DEADBEEFBEEF)}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{-FP64::nan(0x0DEADBEEFBEEF)}.is_arithmetic_nan());
#endif

    // not NaN
    EXPECT_FALSE(FP64{0.0}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{-0.0}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{1.234}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{-1.234}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{FP64::Limits::infinity()}.is_arithmetic_nan());
    EXPECT_FALSE(FP64{-FP64::Limits::infinity()}.is_arithmetic_nan());
}

TEST(floating_point_utils, float_is_canonical_nan)
{
    // canonical
    EXPECT_TRUE(FP32{FP32::nan(FP32::canon)}.is_canonical_nan());
    EXPECT_TRUE(FP32{-FP32::nan(FP32::canon)}.is_canonical_nan());

    // arithmetic
    EXPECT_FALSE(FP32{FP32::nan(FP32::canon + 1)}.is_canonical_nan());
    EXPECT_FALSE(FP32{-FP32::nan(FP32::canon + 1)}.is_canonical_nan());
    EXPECT_FALSE(FP32{FP32::nan(FP32::canon + 0xDEADBEEF)}.is_canonical_nan());
    EXPECT_FALSE(FP32{-FP32::nan(FP32::canon + 0xDEADBEEF)}.is_canonical_nan());
    EXPECT_FALSE(FP32{FP32::nan(FP32::mantissa_mask)}.is_canonical_nan());
    EXPECT_FALSE(FP32{-FP32::nan(FP32::mantissa_mask)}.is_canonical_nan());

    // non-arithmetic
    EXPECT_FALSE(FP32{FP32::nan(1)}.is_canonical_nan());
    EXPECT_FALSE(FP32{-FP32::nan(1)}.is_canonical_nan());
    EXPECT_FALSE(FP32{FP32::nan(0xDEADBEEF)}.is_canonical_nan());
    EXPECT_FALSE(FP32{-FP32::nan(0xDEADBEEF)}.is_canonical_nan());

    // not NaN
    EXPECT_FALSE(FP32{0.0f}.is_canonical_nan());
    EXPECT_FALSE(FP32{-0.0f}.is_canonical_nan());
    EXPECT_FALSE(FP32{1.234f}.is_canonical_nan());
    EXPECT_FALSE(FP32{-1.234f}.is_canonical_nan());
    EXPECT_FALSE(FP32{FP32::Limits::infinity()}.is_canonical_nan());
    EXPECT_FALSE(FP32{-FP32::Limits::infinity()}.is_canonical_nan());
}

TEST(floating_point_utils, float_is_arithmetic_nan)
{
    // canonical
    EXPECT_TRUE(FP32{FP32::nan(FP32::canon)}.is_arithmetic_nan());
    EXPECT_TRUE(FP32{-FP32::nan(FP32::canon)}.is_arithmetic_nan());

    // arithmetic
    EXPECT_TRUE(FP32{FP32::nan(FP32::canon + 1)}.is_arithmetic_nan());
    EXPECT_TRUE(FP32{-FP32::nan(FP32::canon + 1)}.is_arithmetic_nan());
    EXPECT_TRUE(FP32{FP32::nan(FP32::canon + 0xDEADBEEF)}.is_arithmetic_nan());
    EXPECT_TRUE(FP32{-FP32::nan(FP32::canon + 0xDEADBEEF)}.is_arithmetic_nan());
    EXPECT_TRUE(FP32{FP32::nan(FP32::mantissa_mask)}.is_arithmetic_nan());
    EXPECT_TRUE(FP32{-FP32::nan(FP32::mantissa_mask)}.is_arithmetic_nan());

#if SNAN_SUPPORTED
    // non-arithmetic
    EXPECT_FALSE(FP32{FP32::nan(1)}.is_arithmetic_nan());
    EXPECT_FALSE(FP32{-FP32::nan(1)}.is_arithmetic_nan());
    EXPECT_FALSE(FP32{FP32::nan(0xDEADBEEF)}.is_arithmetic_nan());
    EXPECT_FALSE(FP32{-FP32::nan(0xDEADBEEF)}.is_arithmetic_nan());
#endif

    // not NaN
    EXPECT_FALSE(FP32{0.0f}.is_arithmetic_nan());
    EXPECT_FALSE(FP32{-0.0f}.is_arithmetic_nan());
    EXPECT_FALSE(FP32{1.234f}.is_arithmetic_nan());
    EXPECT_FALSE(FP32{-1.234f}.is_arithmetic_nan());
    EXPECT_FALSE(FP32{FP32::Limits::infinity()}.is_arithmetic_nan());
    EXPECT_FALSE(FP32{-FP32::Limits::infinity()}.is_arithmetic_nan());
}

TEST(floating_point_utils, fp32_ostream)
{
    std::ostringstream os;

    os << FP32{-0.0f};
    EXPECT_EQ(os.str(), "-0 [-0x0p+0]");
    os.str({});

    os << FP32::nan(FP32::canon);
    EXPECT_EQ(os.str(), "nan [400000]");
    os.str({});
}

TEST(floating_point_utils, fp64_ostream)
{
    std::ostringstream os;

    os << FP64{-8.125};
    EXPECT_EQ(os.str(), "-8.125 [-0x1.04p+3]");
    os.str({});

    os << FP64::nan(FP64::canon + 1);
    EXPECT_EQ(os.str(), "nan [8000000000001]");
    os.str({});
}
