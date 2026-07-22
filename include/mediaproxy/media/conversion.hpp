#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include <mediaproxy/media/classification.hpp>
#include <mediaproxy/media/mime.hpp>
#include <mediaproxy/media/resize.hpp>

namespace mediaproxy::media {

enum class MediaConversionError {
    none,
    unsupported,
    decode,
    convert,
};

struct MediaConversionResult {
    MediaConversionError error = MediaConversionError::none;
    OutputFormat encoded_format = OutputFormat::webp;
    std::vector<std::byte> body;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == MediaConversionError::none;
    }
};

[[nodiscard]] MediaConversionResult convert_media(
    std::span<const std::byte> body,
    MimeType mime,
    bool force_static,
    OutputFormat preferred_output,
    ImageDimensions limits);

} // namespace mediaproxy::media
