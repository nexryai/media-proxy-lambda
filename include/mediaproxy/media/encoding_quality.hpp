#pragma once

namespace mediaproxy::media {

enum class EncodingQuality {
    standard,
    url_only,
};

[[nodiscard]] auto encoding_quality_value(EncodingQuality quality) noexcept -> int;

} // namespace mediaproxy::media
