#pragma once

namespace mediaproxy::media {

enum class EncodingQuality {
    standard,
    url_only,
};

[[nodiscard]] int vips_encoding_quality(EncodingQuality quality) noexcept;

} // namespace mediaproxy::media
