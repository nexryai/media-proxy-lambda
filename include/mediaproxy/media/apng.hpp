#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace mediaproxy::media {

enum class ApngClassification {
    not_apng,
    palette,
    animated,
};

enum class ApngParseError {
    none,
    signature,
    truncated_chunk,
    chunk_length,
    crc,
    ihdr,
    animation_control,
    frame_control,
    frame_data,
    sequence,
    frame_rectangle,
    delay_denominator,
    blend_operation,
    dispose_operation,
};

struct ApngFrameControl {
    std::uint32_t sequence = 0;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t x_offset = 0;
    std::uint32_t y_offset = 0;
    std::uint16_t delay_numerator = 0;
    std::uint16_t delay_denominator = 0;
    std::uint8_t dispose = 0;
    std::uint8_t blend = 0;
};

struct ApngDescription {
    ApngParseError error = ApngParseError::none;
    std::uint32_t canvas_width = 0;
    std::uint32_t canvas_height = 0;
    std::uint32_t declared_frames = 0;
    std::uint32_t loop_count = 0;
    std::vector<ApngFrameControl> frames;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == ApngParseError::none;
    }
};

[[nodiscard]] ApngClassification classify_apng(
    std::span<const std::byte> body) noexcept;

[[nodiscard]] ApngDescription parse_apng(
    std::span<const std::byte> body);

} // namespace mediaproxy::media
