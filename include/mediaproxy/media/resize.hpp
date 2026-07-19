#pragma once

#include <cstdint>
#include <optional>

namespace mediaproxy::media {

inline constexpr std::uint32_t maximum_media_dimension = 5120;

struct ImageDimensions {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    [[nodiscard]] bool operator==(const ImageDimensions&) const = default;
};

struct AnimatedResize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    [[nodiscard]] bool operator==(const AnimatedResize&) const = default;
};

[[nodiscard]] std::optional<ImageDimensions> validate_dimensions(
    std::int64_t loaded_width,
    std::int64_t loaded_height,
    std::int64_t page_count,
    bool animated) noexcept;

[[nodiscard]] std::optional<double> static_resize_scale(
    ImageDimensions source,
    ImageDimensions limits) noexcept;

[[nodiscard]] std::optional<AnimatedResize> animated_resize_target(
    ImageDimensions source,
    ImageDimensions limits) noexcept;

} // namespace mediaproxy::media
