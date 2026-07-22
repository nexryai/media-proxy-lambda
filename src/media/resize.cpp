#include <mediaproxy/media/resize.hpp>

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace mediaproxy::media {
namespace {

[[nodiscard]] std::optional<std::uint32_t> rounded_dimension(
    double value) noexcept
{
    const double rounded = std::round(value);
    if (!std::isfinite(rounded) || rounded < 1.0
        || rounded > std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }
    return static_cast<std::uint32_t>(rounded);
}

} // namespace

std::optional<ImageDimensions> validate_dimensions(
    std::int64_t loaded_width,
    std::int64_t loaded_height,
    std::int64_t page_count,
    bool animated) noexcept
{
    if (loaded_width <= 0 || loaded_height <= 0 || page_count <= 0) {
        return std::nullopt;
    }

    std::int64_t frame_height = loaded_height;
    if (animated) {
        if (loaded_height % page_count != 0) {
            return std::nullopt;
        }
        frame_height /= page_count;
    }
    if (frame_height <= 0 || loaded_width > maximum_media_width
        || frame_height > maximum_media_height) {
        return std::nullopt;
    }

    return ImageDimensions{
        .width = static_cast<std::uint32_t>(loaded_width),
        .height = static_cast<std::uint32_t>(frame_height),
    };
}

std::optional<double> static_resize_scale(
    ImageDimensions source,
    ImageDimensions limits) noexcept
{
    if (source.width == 0 || source.height == 0 || limits.width == 0
        || limits.height == 0) {
        return std::nullopt;
    }
    if (source.width <= limits.width && source.height <= limits.height) {
        return std::nullopt;
    }

    const std::int64_t width_excess =
        static_cast<std::int64_t>(source.width) - limits.width;
    const std::int64_t height_excess =
        static_cast<std::int64_t>(source.height) - limits.height;
    return width_excess < height_excess
        ? static_cast<double>(limits.height) / source.height
        : static_cast<double>(limits.width) / source.width;
}

std::optional<AnimatedResize> animated_resize_target(
    ImageDimensions source,
    ImageDimensions limits) noexcept
{
    if (source.width == 0 || source.height == 0) {
        return std::nullopt;
    }
    if (source.width <= limits.width && source.height <= limits.height) {
        return std::nullopt;
    }

    const double aspect =
        static_cast<double>(source.width) / source.height;
    const std::int64_t width_excess =
        static_cast<std::int64_t>(source.width) - limits.width;
    const std::int64_t height_excess =
        static_cast<std::int64_t>(source.height) - limits.height;
    if (limits.width != 0 && limits.height != 0
        && source.width > limits.width && source.height > limits.height) {
        if (width_excess < height_excess) {
            limits.width = 0;
        } else {
            limits.height = 0;
        }
    }

    AnimatedResize target{source.width, source.height};
    if (limits.width != 0 && source.width > limits.width) {
        const auto height = rounded_dimension(limits.width / aspect);
        if (!height.has_value()) {
            return std::nullopt;
        }
        target = {limits.width, *height};
    } else if (limits.width == 0 && limits.height != 0
        && source.height > limits.height) {
        const auto width = rounded_dimension(limits.height * aspect);
        if (!width.has_value()) {
            return std::nullopt;
        }
        target = {*width, limits.height};
    }
    return target;
}

} // namespace mediaproxy::media
