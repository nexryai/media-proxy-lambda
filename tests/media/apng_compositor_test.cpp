#include <cstddef>
#include <cstdint>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/media/apng_compositor.hpp>

namespace {

using mediaproxy::media::apng_frame_duration_ms;
using mediaproxy::media::ApngCompositionError;
using mediaproxy::media::ApngFrameControl;
using mediaproxy::media::compose_apng_frame;

auto pixels(std::initializer_list<std::uint8_t> values) -> std::vector<std::byte> {
    std::vector<std::byte> result;
    result.reserve(values.size());
    for (const auto value : values) {
        result.push_back(static_cast<std::byte>(value));
    }

    return result;
}

auto frame_control(std::uint8_t dispose, std::uint8_t blend) -> ApngFrameControl {

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

TEST(ApngCompositor, SourceOverUsesPriorCanvasAtOffset) {
    auto canvas = pixels({0, 0, 255, 255, 0, 0, 255, 255});
    const auto half_red = pixels({255, 0, 0, 128});
    const auto result = compose_apng_frame(canvas, 2, 1, frame_control(0, 1), half_red);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.displayed_rgba, pixels({0, 0, 255, 255, 128, 0, 127, 255}));
    EXPECT_EQ(canvas, result.displayed_rgba);
}

TEST(ApngCompositor, SourceClearsAndReplacesOnlyFrameRectangle) {
    auto canvas = pixels({0, 255, 0, 255, 0, 0, 255, 255});
    const auto transparent = pixels({0, 0, 0, 0});
    const auto result = compose_apng_frame(canvas, 2, 1, frame_control(0, 0), transparent);
    ASSERT_TRUE(result);
    EXPECT_EQ(result.displayed_rgba, pixels({0, 255, 0, 255, 0, 0, 0, 0}));
}

TEST(ApngCompositor, AppliesBackgroundAndPreviousAfterDisplayCapture) {
    const auto red = pixels({255, 0, 0, 255});

    auto background_canvas = pixels({0, 255, 0, 255, 0, 0, 255, 255});
    const auto background = compose_apng_frame(background_canvas, 2, 1, frame_control(1, 0), red);
    ASSERT_TRUE(background);
    EXPECT_EQ(background.displayed_rgba, pixels({0, 255, 0, 255, 255, 0, 0, 255}));
    EXPECT_EQ(background_canvas, pixels({0, 255, 0, 255, 0, 0, 0, 0}));

    auto previous_canvas = pixels({0, 255, 0, 255, 0, 0, 255, 255});
    const auto previous = compose_apng_frame(previous_canvas, 2, 1, frame_control(2, 0), red);
    ASSERT_TRUE(previous);
    EXPECT_EQ(previous.displayed_rgba, background.displayed_rgba);
    EXPECT_EQ(previous_canvas, pixels({0, 255, 0, 255, 0, 0, 255, 255}));
}

TEST(ApngCompositor, RejectsInvalidInputBeforeChangingCanvas) {
    auto canvas = pixels({0, 0, 0, 0});
    const auto original = canvas;
    auto control = frame_control(0, 0);
    control.x_offset = 1;
    const auto result = compose_apng_frame(canvas, 1, 1, control, pixels({255, 0, 0, 255}));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, ApngCompositionError::frame_rectangle);
    EXPECT_EQ(canvas, original);
}

TEST(ApngCompositor, TruncatesFrameDelayToMilliseconds) {
    EXPECT_EQ(apng_frame_duration_ms(1, 10), 100);
    EXPECT_EQ(apng_frame_duration_ms(1, 3), 333);
    EXPECT_EQ(apng_frame_duration_ms(5, 1), 5000);
}

} // namespace
