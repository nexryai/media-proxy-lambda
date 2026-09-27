#include <mediaproxy/media/classification.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

#include <jxl/decode.h>

namespace mediaproxy::media {
namespace {

[[nodiscard]] auto read_u32_be(std::span<const std::byte> body, std::size_t offset) noexcept -> std::uint32_t {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < 4; ++index) {
        value = (value << 8U) | std::to_integer<std::uint8_t>(body[offset + index]);
    }

    return value;
}

[[nodiscard]] auto matches_ascii(std::span<const std::byte> body, std::size_t offset, std::string_view value) noexcept -> bool {
    if (offset > body.size() || value.size() > body.size() - offset) {
        return false;
    }
    for (std::size_t index = 0; index < value.size(); ++index) {
        if (std::to_integer<unsigned char>(body[offset + index]) != static_cast<unsigned char>(value[index])) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] auto is_animated_webp(std::span<const std::byte> body) noexcept -> bool {
    constexpr std::size_t animation_tag_offset = 0x1e;
    constexpr std::size_t animation_tag_size = 4;
    if (body.size() < animation_tag_offset + animation_tag_size) {
        return false;
    }

    constexpr char animation_tag[] = "ANIM";
    for (std::size_t index = 0; index < animation_tag_size; ++index) {
        if (std::to_integer<unsigned char>(body[animation_tag_offset + index]) != static_cast<unsigned char>(animation_tag[index])) {
            return false;
        }
    }

    return true;
}

} // namespace

auto is_animated_avif(std::span<const std::byte> body) noexcept -> bool {
    constexpr std::size_t minimum_ftyp_size = 16;
    constexpr std::size_t brand_size = 4;
    if (body.size() < minimum_ftyp_size) {
        return false;
    }

    const std::size_t box_size = read_u32_be(body, 0);
    if (box_size < minimum_ftyp_size || box_size > body.size() || !matches_ascii(body, 4, "ftyp")) {
        return false;
    }

    if (matches_ascii(body, 8, "avis")) {
        return true;
    }

    for (std::size_t offset = minimum_ftyp_size; offset <= box_size - brand_size; offset += brand_size) {
        if (matches_ascii(body, offset, "avis")) {
            return true;
        }
    }

    return false;
}

auto is_animated_jxl(std::span<const std::byte> body) noexcept -> bool {
    if (body.empty()) {
        return false;
    }
    std::unique_ptr<JxlDecoder, decltype(&JxlDecoderDestroy)> decoder(JxlDecoderCreate(nullptr), &JxlDecoderDestroy);
    if (!decoder) {
        return false;
    }
    const bool subscribed = JxlDecoderSubscribeEvents(decoder.get(), JXL_DEC_BASIC_INFO) == JXL_DEC_SUCCESS;
    bool animated = false;
    if (subscribed) {
        JxlDecoderSetInput(decoder.get(), reinterpret_cast<const std::uint8_t *>(body.data()), body.size());
        JxlDecoderCloseInput(decoder.get());
        if (JxlDecoderProcessInput(decoder.get()) == JXL_DEC_BASIC_INFO) {
            JxlBasicInfo info{};
            animated = JxlDecoderGetBasicInfo(decoder.get(), &info) == JXL_DEC_SUCCESS && info.have_animation == JXL_TRUE;
        }
    }

    return animated;
}

auto is_convertible_mime(MimeType mime) noexcept -> bool {
    switch (mime) {
        case MimeType::image_avif:
        case MimeType::image_ico:
        case MimeType::image_jpeg:
        case MimeType::image_jxl:
        case MimeType::image_svg_xml:
        case MimeType::image_png:
        case MimeType::image_webp:
        case MimeType::image_gif:
        case MimeType::image_x_icon:
            return true;
        default:
            return false;
    }
}

auto classify_media(MimeType mime, std::span<const std::byte> body, bool force_static, OutputFormat preferred_output) noexcept -> std::optional<MediaPlan> {
    if (!is_convertible_mime(mime)) {
        return std::nullopt;
    }

    const bool animated = !force_static && (mime == MimeType::image_gif || (mime == MimeType::image_avif && is_animated_avif(body)) || (mime == MimeType::image_jxl && is_animated_jxl(body)) || (mime == MimeType::image_webp && is_animated_webp(body)));

    return MediaPlan{
        .animated = animated,
        .output = animated ? OutputFormat::webp : preferred_output,
    };
}

} // namespace mediaproxy::media
