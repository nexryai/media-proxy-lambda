#include <mediaproxy/media/apng_compositor.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace mediaproxy::media {
namespace {

[[nodiscard]] bool rgba_size(
    std::uint32_t width,
    std::uint32_t height,
    std::size_t& output) noexcept
{
    constexpr std::size_t channels = 4;
    if (width == 0 || height == 0
        || width > std::numeric_limits<std::size_t>::max() / height) {
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    if (pixels > std::numeric_limits<std::size_t>::max() / channels) {
        return false;
    }
    output = pixels * channels;
    return true;
}

[[nodiscard]] std::size_t pixel_offset(
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t width) noexcept
{
    return (static_cast<std::size_t>(y) * width + x) * 4;
}

void clear_rectangle(
    std::vector<std::byte>& canvas,
    std::uint32_t canvas_width,
    const ApngFrameControl& control)
{
    for (std::uint32_t y = 0; y < control.height; ++y) {
        const std::size_t begin = pixel_offset(
            control.x_offset, control.y_offset + y, canvas_width);
        std::fill_n(canvas.begin() + static_cast<std::ptrdiff_t>(begin),
            static_cast<std::size_t>(control.width) * 4, std::byte{0});
    }
}

void source_copy(
    std::vector<std::byte>& canvas,
    std::uint32_t canvas_width,
    const ApngFrameControl& control,
    std::span<const std::byte> frame)
{
    const std::size_t row_size = static_cast<std::size_t>(control.width) * 4;
    for (std::uint32_t y = 0; y < control.height; ++y) {
        const std::size_t source = static_cast<std::size_t>(y) * row_size;
        const std::size_t destination = pixel_offset(
            control.x_offset, control.y_offset + y, canvas_width);
        std::copy_n(frame.begin() + static_cast<std::ptrdiff_t>(source),
            row_size,
            canvas.begin() + static_cast<std::ptrdiff_t>(destination));
    }
}

[[nodiscard]] std::uint8_t rounded_divide(
    std::uint32_t numerator,
    std::uint32_t denominator) noexcept
{
    return static_cast<std::uint8_t>((numerator + denominator / 2U)
        / denominator);
}

void source_over_pixel(std::byte* destination, const std::byte* source)
{
    const std::uint32_t source_alpha =
        std::to_integer<std::uint8_t>(source[3]);
    const std::uint32_t destination_alpha =
        std::to_integer<std::uint8_t>(destination[3]);
    if (source_alpha == 0) {
        return;
    }
    if (source_alpha == 255 || destination_alpha == 0) {
        std::copy_n(source, 4, destination);
        return;
    }

    const std::uint32_t inverse_source_alpha = 255 - source_alpha;
    const std::uint32_t alpha_numerator = source_alpha * 255
        + destination_alpha * inverse_source_alpha;
    for (std::size_t channel = 0; channel < 3; ++channel) {
        const std::uint32_t source_color =
            std::to_integer<std::uint8_t>(source[channel]);
        const std::uint32_t destination_color =
            std::to_integer<std::uint8_t>(destination[channel]);
        const std::uint32_t color_numerator =
            source_color * source_alpha * 255
            + destination_color * destination_alpha * inverse_source_alpha;
        destination[channel] = static_cast<std::byte>(
            rounded_divide(color_numerator, alpha_numerator));
    }
    destination[3] = static_cast<std::byte>(
        rounded_divide(alpha_numerator, 255));
}

void source_over(
    std::vector<std::byte>& canvas,
    std::uint32_t canvas_width,
    const ApngFrameControl& control,
    std::span<const std::byte> frame)
{
    for (std::uint32_t y = 0; y < control.height; ++y) {
        for (std::uint32_t x = 0; x < control.width; ++x) {
            const std::size_t source = pixel_offset(x, y, control.width);
            const std::size_t destination = pixel_offset(control.x_offset + x,
                control.y_offset + y, canvas_width);
            source_over_pixel(canvas.data() + destination,
                frame.data() + source);
        }
    }
}

[[nodiscard]] ApngComposedFrame fail(ApngCompositionError error)
{
    return {.error = error, .displayed_rgba = {}};
}

} // namespace

ApngComposedFrame compose_apng_frame(
    std::vector<std::byte>& canvas_rgba,
    std::uint32_t canvas_width,
    std::uint32_t canvas_height,
    const ApngFrameControl& control,
    std::span<const std::byte> frame_rgba)
{
    std::size_t canvas_size = 0;
    if (!rgba_size(canvas_width, canvas_height, canvas_size)
        || canvas_rgba.size() != canvas_size) {
        return fail(ApngCompositionError::canvas_size);
    }
    std::size_t frame_size = 0;
    if (!rgba_size(control.width, control.height, frame_size)
        || frame_rgba.size() != frame_size) {
        return fail(ApngCompositionError::frame_size);
    }
    if (control.x_offset > canvas_width
        || control.width > canvas_width - control.x_offset
        || control.y_offset > canvas_height
        || control.height > canvas_height - control.y_offset) {
        return fail(ApngCompositionError::frame_rectangle);
    }
    if (control.blend > 1) {
        return fail(ApngCompositionError::blend_operation);
    }
    if (control.dispose > 2) {
        return fail(ApngCompositionError::dispose_operation);
    }

    const std::vector previous_canvas = canvas_rgba;
    if (control.blend == 0) {
        clear_rectangle(canvas_rgba, canvas_width, control);
        source_copy(canvas_rgba, canvas_width, control, frame_rgba);
    } else {
        source_over(canvas_rgba, canvas_width, control, frame_rgba);
    }

    ApngComposedFrame result{
        .error = ApngCompositionError::none,
        .displayed_rgba = canvas_rgba,
    };
    if (control.dispose == 1) {
        clear_rectangle(canvas_rgba, canvas_width, control);
    } else if (control.dispose == 2) {
        canvas_rgba = previous_canvas;
    }
    return result;
}

std::int32_t apng_frame_timestamp_ms(
    std::uint32_t callback_number,
    std::uint16_t delay_numerator,
    std::uint16_t delay_denominator) noexcept
{
    if (delay_denominator == 0) {
        return 0;
    }
    const float seconds = static_cast<float>(delay_numerator)
        / static_cast<float>(delay_denominator);
    const float callback_seconds =
        (static_cast<float>(callback_number) + 1.0F) * seconds;
    const float milliseconds = callback_seconds * 1000.0F;
    if (!std::isfinite(milliseconds)
        || milliseconds > static_cast<float>(
            std::numeric_limits<std::int32_t>::max())) {
        return std::numeric_limits<std::int32_t>::max();
    }
    return static_cast<std::int32_t>(milliseconds);
}

} // namespace mediaproxy::media
