#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <mediaproxy/media/apng.hpp>

namespace mediaproxy::media {

enum class ApngDecodeError {
    none,
    parse,
    frame_stream,
    png_decode,
    dimensions,
};

struct ApngDecodedFrame {
    ApngFrameControl control;
    std::vector<std::byte> rgba;
};

struct ApngDecodedAnimation {
    ApngDecodeError error = ApngDecodeError::none;
    std::uint32_t canvas_width = 0;
    std::uint32_t canvas_height = 0;
    std::vector<ApngDecodedFrame> frames;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == ApngDecodeError::none;
    }
};

[[nodiscard]] ApngDecodedAnimation decode_apng_frames(
    std::span<const std::byte> body);

} // namespace mediaproxy::media
