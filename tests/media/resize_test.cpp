#include <gtest/gtest.h>
#include <mediaproxy/media/resize.hpp>

namespace {

using mediaproxy::media::AnimatedResize;
using mediaproxy::media::ImageDimensions;
using mediaproxy::media::animated_resize_target;
using mediaproxy::media::static_resize_scale;
using mediaproxy::media::validate_dimensions;

TEST(MediaDimensions, ValidatesStaticAndAnimatedPageGeometry)
{
    EXPECT_EQ(validate_dimensions(640, 480, 1, false),
        (ImageDimensions{640, 480}));
    EXPECT_EQ(validate_dimensions(640, 1440, 3, true),
        (ImageDimensions{640, 480}));

    EXPECT_FALSE(validate_dimensions(0, 480, 1, false).has_value());
    EXPECT_FALSE(validate_dimensions(640, 0, 1, false).has_value());
    EXPECT_FALSE(validate_dimensions(640, 480, 0, true).has_value());
    EXPECT_FALSE(validate_dimensions(640, 1000, 3, true).has_value());
    EXPECT_EQ(validate_dimensions(7680, 4320, 1, false),
        (ImageDimensions{7680, 4320}));
    EXPECT_FALSE(validate_dimensions(7681, 4320, 1, false).has_value());
    EXPECT_FALSE(validate_dimensions(7680, 4321, 1, false).has_value());
    EXPECT_FALSE(validate_dimensions(640, 8642, 2, true).has_value());
    EXPECT_TRUE(validate_dimensions(7680, 8640, 2, true).has_value());
}

TEST(StaticResize, PreservesAbsoluteExcessSelection)
{
    constexpr ImageDimensions limits{500, 400};
    EXPECT_FALSE(static_resize_scale({500, 400}, limits).has_value());
    EXPECT_DOUBLE_EQ(*static_resize_scale({1000, 300}, limits), 0.5);
    EXPECT_DOUBLE_EQ(*static_resize_scale({300, 800}, limits), 0.5);
    EXPECT_DOUBLE_EQ(*static_resize_scale({700, 1000}, limits), 0.4);
    EXPECT_DOUBLE_EQ(*static_resize_scale({1200, 600}, limits), 0.5 / 1.2);
    EXPECT_DOUBLE_EQ(*static_resize_scale({700, 600}, limits), 5.0 / 7.0);
}

TEST(StaticResize, AbsoluteExcessCanLeaveASecondLimitExceeded)
{
    const double scale =
        *static_resize_scale({100, 1000}, {50, 900});
    EXPECT_DOUBLE_EQ(scale, 0.9);
    EXPECT_GT(100.0 * scale, 50.0);
}

TEST(AnimatedResize, PreservesCompatibilityBranchesAndRounding)
{
    constexpr ImageDimensions limits{500, 400};
    EXPECT_FALSE(animated_resize_target({500, 400}, limits).has_value());
    EXPECT_EQ(animated_resize_target({1000, 300}, limits),
        (AnimatedResize{500, 150}));
    EXPECT_EQ(animated_resize_target({300, 800}, limits),
        (AnimatedResize{300, 800}));
    EXPECT_EQ(animated_resize_target({700, 1000}, limits),
        (AnimatedResize{280, 400}));
    EXPECT_EQ(animated_resize_target({1200, 600}, limits),
        (AnimatedResize{500, 250}));
    EXPECT_EQ(animated_resize_target({700, 600}, limits),
        (AnimatedResize{500, 429}));
    EXPECT_EQ(animated_resize_target({3, 2}, {1, 2}),
        (AnimatedResize{1, 1}));
}

TEST(AnimatedResize, SupportsZeroLimitSentinelWithoutDivisionByZero)
{
    EXPECT_EQ(animated_resize_target({301, 800}, {0, 400}),
        (AnimatedResize{151, 400}));
    EXPECT_EQ(animated_resize_target({1000, 301}, {500, 0}),
        (AnimatedResize{500, 151}));
}

} // namespace
