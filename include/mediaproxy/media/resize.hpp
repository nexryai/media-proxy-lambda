#pragma once

#include <cstdint>
#include <optional>

namespace mediaproxy::media {

inline constexpr std::uint32_t maximum_media_width = 7680;
inline constexpr std::uint32_t maximum_media_height = 4320;

struct ImageDimensions {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    [[nodiscard]] auto operator==(const ImageDimensions &) const -> bool = default;
};

struct AnimatedResize {
    std::uint32_t width = 0;
    std::uint32_t height = 0;

    [[nodiscard]] auto operator==(const AnimatedResize &) const -> bool = default;
};

[[nodiscard]] auto validate_dimensions(std::int64_t loaded_width, std::int64_t loaded_height, std::int64_t page_count, bool animated) noexcept -> std::optional<ImageDimensions>;

[[nodiscard]] auto static_resize_scale(ImageDimensions source, ImageDimensions limits) noexcept -> std::optional<double>;

[[nodiscard]] auto animated_resize_target(ImageDimensions source, ImageDimensions limits) noexcept -> std::optional<AnimatedResize>;

} // namespace mediaproxy::media
