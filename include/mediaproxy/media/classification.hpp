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

    [[nodiscard]] bool operator==(const MediaPlan&) const = default;
};

[[nodiscard]] bool is_convertible_mime(MimeType mime) noexcept;

[[nodiscard]] std::optional<MediaPlan> classify_media(
    MimeType mime,
    std::span<const std::byte> body,
    bool force_static,
    OutputFormat preferred_output) noexcept;

} // namespace mediaproxy::media
