#include <mediaproxy/media/classification.hpp>

#include <cstddef>
#include <span>

namespace mediaproxy::media {
namespace {

[[nodiscard]] bool is_animated_webp(
    std::span<const std::byte> body) noexcept
{
    constexpr std::size_t animation_tag_offset = 0x1e;
    constexpr std::size_t animation_tag_size = 4;
    if (body.size() < animation_tag_offset + animation_tag_size) {
        return false;
    }

    constexpr char animation_tag[] = "ANIM";
    for (std::size_t index = 0; index < animation_tag_size; ++index) {
        if (std::to_integer<unsigned char>(body[animation_tag_offset + index])
            != static_cast<unsigned char>(animation_tag[index])) {
            return false;
        }
    }
    return true;
}

} // namespace

bool is_convertible_mime(MimeType mime) noexcept
{
    switch (mime) {
    case MimeType::image_avif:
    case MimeType::image_ico:
    case MimeType::image_jpeg:
    case MimeType::image_png:
    case MimeType::image_webp:
    case MimeType::image_gif:
    case MimeType::image_x_icon:
        return true;
    default:
        return false;
    }
}

std::optional<MediaPlan> classify_media(
    MimeType mime,
    std::span<const std::byte> body,
    bool force_static,
    OutputFormat preferred_output) noexcept
{
    if (!is_convertible_mime(mime)) {
        return std::nullopt;
    }

    const bool animated = !force_static
        && (mime == MimeType::image_gif
            || (mime == MimeType::image_webp && is_animated_webp(body)));
    return MediaPlan{
        .animated = animated,
        .output = animated ? OutputFormat::webp : preferred_output,
    };
}

} // namespace mediaproxy::media
