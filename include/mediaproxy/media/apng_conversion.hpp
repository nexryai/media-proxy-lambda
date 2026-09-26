#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include <mediaproxy/media/encoding_quality.hpp>
#include <webp/encode.h>

namespace mediaproxy::media {

enum class ApngConversionError {
    none,
    decode,
    base_frame,
    dimensions,
    composition,
    picture,
    encoder,
};

struct ApngConversionResult {
    ApngConversionError error = ApngConversionError::none;
    std::vector<std::byte> body;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == ApngConversionError::none;
    }
};

[[nodiscard]] bool initialize_apng_webp_config(
    WebPConfig& config,
    EncodingQuality quality) noexcept;

[[nodiscard]] ApngConversionResult convert_apng_to_webp(
    std::span<const std::byte> body,
    std::uint32_t target_width,
    std::uint32_t target_height,
    EncodingQuality quality = EncodingQuality::standard);

} // namespace mediaproxy::media
