#pragma once

namespace mediaproxy::media {

enum class EncodingQuality {
    standard,
    url_only,
};

[[nodiscard]] int encoding_quality_value(EncodingQuality quality) noexcept;

} // namespace mediaproxy::media
