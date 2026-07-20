#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <mediaproxy/media/apng.hpp>

namespace mediaproxy::media {

enum class ApngCompositionError {
    none,
    canvas_size,
    frame_size,
    frame_rectangle,
    blend_operation,
    dispose_operation,
};

struct ApngComposedFrame {
    ApngCompositionError error = ApngCompositionError::none;
    std::vector<std::byte> displayed_rgba;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == ApngCompositionError::none;
    }
};

[[nodiscard]] ApngComposedFrame compose_apng_frame(
    std::vector<std::byte>& canvas_rgba,
    std::uint32_t canvas_width,
    std::uint32_t canvas_height,
    const ApngFrameControl& control,
    std::span<const std::byte> frame_rgba);

[[nodiscard]] std::int32_t apng_frame_timestamp_ms(
    std::uint32_t callback_number,
    std::uint16_t delay_numerator,
    std::uint16_t delay_denominator) noexcept;

} // namespace mediaproxy::media
