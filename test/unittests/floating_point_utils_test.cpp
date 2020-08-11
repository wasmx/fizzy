// Fizzy: A fast WebAssembly interpreter
// Copyright 2020 The Fizzy Authors.
// SPDX-License-Identifier: Apache-2.0

#include <gtest/gtest.h>
#include <test/utils/floating_point_utils.hpp>

using namespace fizzy::test;

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
    EXPECT_EQ(FP(FP64::Limits::signaling_NaN()).nan_payload(), 0x4000000000000);

    EXPECT_EQ(FP(FP32::Limits::quiet_NaN()).as_uint(), 0x7FC00000);
    EXPECT_EQ(FP(FP32::Limits::quiet_NaN()).nan_payload(), 0x400000);
    EXPECT_EQ(FP(FP32::Limits::signaling_NaN()).nan_payload(), 0x200000);
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
    EXPECT_EQ(FP(uint64_t{0x0000000000000000}).value, 0.0);
    EXPECT_EQ(FP(uint64_t{0x8000000000000000}).value, -0.0);
    EXPECT_EQ(FP(uint64_t{0x3FF'000000000DEAD}).value, 0x1.000000000DEADp0);
    EXPECT_EQ(FP(uint64_t{0xBFF'000000000DEAD}).value, -0x1.000000000DEADp0);
    EXPECT_EQ(FP(uint64_t{0x7FF'0000000000000}).value, FP64::Limits::infinity());
    EXPECT_EQ(FP(uint64_t{0xFFF'0000000000000}).value, -FP64::Limits::infinity());
}

TEST(floating_point_utils, float_from_uint)
{
    EXPECT_EQ(FP(uint32_t{0x00000000}).value, 0.0f);
    EXPECT_EQ(FP(uint32_t{0x80000000}).value, -0.0f);
    EXPECT_EQ(FP(uint32_t{0x3FEF5680}).value, 0x1.DEADp0f);
    EXPECT_EQ(FP(uint32_t{0xBFEF5680}).value, -0x1.DEADp0f);
    EXPECT_EQ(FP(uint32_t{0x7F800000}).value, FP32::Limits::infinity());
    EXPECT_EQ(FP(uint32_t{0xFF800000}).value, -FP32::Limits::infinity());
}

TEST(floating_point_utils, double_nan_payload)
{
    constexpr auto inf = FP64::Limits::infinity();
    const auto qnan = FP64::nan(FP64::canon);

    EXPECT_EQ(FP(0.0).nan_payload(), 0);
    EXPECT_EQ(FP(FP64::nan(1)).nan_payload(), 1);
    EXPECT_EQ(FP(FP64::nan(FP64::canon + 1)).nan_payload(), FP64::canon + 1);
    EXPECT_EQ(FP(qnan).nan_payload(), FP64::canon);
    EXPECT_EQ(FP(qnan + 1.0).nan_payload(), FP64::canon);
    EXPECT_EQ(FP(inf - inf).nan_payload(), FP64::canon);
    EXPECT_EQ(FP(inf * 0.0).nan_payload(), FP64::canon);
}

TEST(floating_point_utils, float_nan_payload)
{
    constexpr auto inf = FP32::Limits::infinity();
    const auto qnan = FP32::nan(FP32::canon);

    EXPECT_EQ(FP(0.0f).nan_payload(), 0);
    EXPECT_EQ(FP(FP32::nan(1)).nan_payload(), 1);
    EXPECT_EQ(FP(FP32::nan(FP32::canon + 1)).nan_payload(), FP32::canon + 1);
    EXPECT_EQ(FP(qnan).nan_payload(), FP32::canon);
    EXPECT_EQ(FP(qnan + 1.0f).nan_payload(), FP32::canon);
    EXPECT_EQ(FP(inf - inf).nan_payload(), FP32::canon);
    EXPECT_EQ(FP(inf * 0.0f).nan_payload(), FP32::canon);
}

TEST(floating_point_utils, double_nan)
{
    EXPECT_TRUE(std::isnan(FP64::nan(FP64::canon)));
    EXPECT_TRUE(std::isnan(FP64::nan(1)));
    EXPECT_TRUE(std::isnan(FP64::nan(0xDEADBEEF)));
    EXPECT_TRUE(std::isnan(FP64::nan(0xDEADBEEFBEEEF)));
    EXPECT_FALSE(std::isnan(FP64::nan(0)));

    EXPECT_EQ(FP{FP64::nan(FP64::canon)}.nan_payload(), FP64::canon);

    EXPECT_EQ(FP{FP64::nan(FP64::canon)}.as_uint(), 0x7FF'8000000000000);
    EXPECT_EQ(FP{FP64::nan(0xDEADBEEF)}.as_uint(), 0x7FF'00000DEADBEEF);
    EXPECT_EQ(FP{FP64::nan(0xDEADBEEFBEEEF)}.as_uint(), 0x7FF'DEADBEEFBEEEF);
}

TEST(floating_point_utils, float_nan)
{
    EXPECT_TRUE(std::isnan(FP32::nan(FP32::canon)));
    EXPECT_TRUE(std::isnan(FP32::nan(1)));
    EXPECT_TRUE(std::isnan(FP32::nan(0x7fffff)));
    EXPECT_TRUE(std::isnan(FP32::nan(0x400001)));
    EXPECT_FALSE(std::isnan(FP32::nan(0)));

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
