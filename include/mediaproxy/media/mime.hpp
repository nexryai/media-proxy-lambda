#pragma once

#include <cstddef>
#include <span>
#include <string_view>

namespace mediaproxy::media {

inline constexpr std::size_t maximum_mime_sample_bytes = 512;

enum class MimeType {
    image_avif,
    image_ico,
    image_x_icon,
    image_bmp,
    image_gif,
    image_webp,
    image_png,
    image_jpeg,
    application_pdf,
    application_postscript,
    audio_mpeg,
    application_ogg,
    video_webm,
    video_avi,
    audio_wave,
    application_zip,
    application_x_gzip,
    application_wasm,
    text_html_utf8,
    text_xml_utf8,
    text_plain_utf8,
    text_plain_utf16be,
    text_plain_utf16le,
    video_mp4,
    font_ttf,
    font_otf,
    font_collection,
    font_woff,
    font_woff2,
    application_eot,
    application_octet_stream,
};

[[nodiscard]] MimeType sniff_mime(
    std::span<const std::byte> body) noexcept;
[[nodiscard]] std::string_view mime_type_name(MimeType type) noexcept;

} // namespace mediaproxy::media
