#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/media/apng_compositor.hpp>

namespace {

using mediaproxy::media::ApngCompositionError;
using mediaproxy::media::ApngFrameControl;
using mediaproxy::media::apng_frame_timestamp_ms;
using mediaproxy::media::compose_apng_frame;

std::vector<std::byte> Pixels(
    std::initializer_list<std::uint8_t> values)
{
    std::vector<std::byte> result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(static_cast<std::byte>(value));
    }
    return result;
}

ApngFrameControl Control(
    std::uint8_t dispose,
    std::uint8_t blend)
{
    return {
        .sequence = 1,
        .width = 1,
        .height = 1,
        .x_offset = 1,
        .y_offset = 0,
        .delay_numerator = 1,
        .delay_denominator = 10,
        .dispose = dispose,
        .blend = blend,
    };
}

TEST(ApngCompositor, SourceOverUsesPriorCanvasAtOffset)
{
    auto canvas = Pixels({0, 0, 255, 255, 0, 0, 255, 255});
    const auto half_red = Pixels({255, 0, 0, 128});
    const auto result = compose_apng_frame(
        canvas, 2, 1, Control(0, 1), half_red);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.displayed_rgba,
        Pixels({0, 0, 255, 255, 128, 0, 127, 255}));
    EXPECT_EQ(canvas, result.displayed_rgba);
}

TEST(ApngCompositor, SourceClearsAndReplacesOnlyFrameRectangle)
{
    auto canvas = Pixels({0, 255, 0, 255, 0, 0, 255, 255});
    const auto transparent = Pixels({0, 0, 0, 0});
    const auto result = compose_apng_frame(
        canvas, 2, 1, Control(0, 0), transparent);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.displayed_rgba,
        Pixels({0, 255, 0, 255, 0, 0, 0, 0}));
}

TEST(ApngCompositor, AppliesBackgroundAndPreviousAfterDisplayCapture)
{
    const auto red = Pixels({255, 0, 0, 255});

    auto background_canvas =
        Pixels({0, 255, 0, 255, 0, 0, 255, 255});
    const auto background = compose_apng_frame(
        background_canvas, 2, 1, Control(1, 0), red);
    ASSERT_TRUE(background);
    EXPECT_EQ(background.displayed_rgba,
        Pixels({0, 255, 0, 255, 255, 0, 0, 255}));
    EXPECT_EQ(background_canvas,
        Pixels({0, 255, 0, 255, 0, 0, 0, 0}));

    auto previous_canvas =
        Pixels({0, 255, 0, 255, 0, 0, 255, 255});
    const auto previous = compose_apng_frame(
        previous_canvas, 2, 1, Control(2, 0), red);
    ASSERT_TRUE(previous);
    EXPECT_EQ(previous.displayed_rgba, background.displayed_rgba);
    EXPECT_EQ(previous_canvas,
        Pixels({0, 255, 0, 255, 0, 0, 255, 255}));
}

TEST(ApngCompositor, RejectsInvalidInputBeforeChangingCanvas)
{
    auto canvas = Pixels({0, 0, 0, 0});
    const auto original = canvas;
    auto control = Control(0, 0);
    control.x_offset = 1;
    const auto result = compose_apng_frame(
        canvas, 1, 1, control, Pixels({255, 0, 0, 255}));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, ApngCompositionError::frame_rectangle);
    EXPECT_EQ(canvas, original);
}

TEST(ApngCompositor, PreservesNonCumulativeFloat32Timestamp)
{
    EXPECT_EQ(apng_frame_timestamp_ms(1, 1, 10), 200);
    EXPECT_EQ(apng_frame_timestamp_ms(2, 1, 10), 300);
    EXPECT_EQ(apng_frame_timestamp_ms(2, 1, 3), 1000);
}

} // namespace
