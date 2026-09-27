#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <mediaproxy/media/encoding_quality.hpp>
#include <mediaproxy/media/resize.hpp>

namespace mediaproxy::media {

enum class AnimatedConversionError {
    none,
    initialization,
    decode,
    dimensions,
    resize,
    encode,
};

struct AnimatedConversionResult {
    AnimatedConversionError error = AnimatedConversionError::none;
    std::vector<std::byte> body;

    [[nodiscard]] explicit operator bool() const noexcept {

        return error == AnimatedConversionError::none;
    }
};

[[nodiscard]] auto convert_animated_image(std::span<const std::byte> body, ImageDimensions limits, EncodingQuality quality = EncodingQuality::standard) -> AnimatedConversionResult;

[[nodiscard]] auto convert_animated_avif(std::span<const std::byte> body, ImageDimensions limits, EncodingQuality quality = EncodingQuality::standard) -> AnimatedConversionResult;

[[nodiscard]] auto convert_animated_jxl(std::span<const std::byte> body, ImageDimensions limits, EncodingQuality quality = EncodingQuality::standard) -> AnimatedConversionResult;

} // namespace mediaproxy::media
