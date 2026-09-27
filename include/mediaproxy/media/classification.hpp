#pragma once

#include <cstddef>
#include <optional>
#include <span>

#include <mediaproxy/media/mime.hpp>

namespace mediaproxy::media {

enum class OutputFormat {
    avif,
    webp,
};

struct MediaPlan {
    bool animated = false;
    OutputFormat output = OutputFormat::webp;

    [[nodiscard]] auto operator==(const MediaPlan &) const -> bool = default;
};

[[nodiscard]] auto is_convertible_mime(MimeType mime) noexcept -> bool;

[[nodiscard]] auto is_animated_avif(std::span<const std::byte> body) noexcept -> bool;

[[nodiscard]] auto is_animated_jxl(std::span<const std::byte> body) noexcept -> bool;

[[nodiscard]] auto classify_media(MimeType mime, std::span<const std::byte> body, bool force_static, OutputFormat preferred_output) noexcept -> std::optional<MediaPlan>;

} // namespace mediaproxy::media
