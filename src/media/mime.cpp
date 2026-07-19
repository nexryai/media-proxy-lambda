#include <mediaproxy/media/mime.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <span>
#include <string_view>

namespace mediaproxy::media {
namespace {

[[nodiscard]] unsigned char octet(std::byte value) noexcept
{
    return std::to_integer<unsigned char>(value);
}

[[nodiscard]] bool starts_with(
    std::span<const std::byte> sample,
    std::string_view signature) noexcept
{
    if (sample.size() < signature.size()) {
        return false;
    }
    for (std::size_t index = 0; index < signature.size(); ++index) {
        if (octet(sample[index])
            != static_cast<unsigned char>(signature[index])) {
            return false;
        }
    }
    return true;
}

template <std::size_t Size>
[[nodiscard]] bool starts_with(
    std::span<const std::byte> sample,
    const std::array<unsigned char, Size>& signature) noexcept
{
    if (sample.size() < signature.size()) {
        return false;
    }
    for (std::size_t index = 0; index < signature.size(); ++index) {
        if (octet(sample[index]) != signature[index]) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] bool ascii_whitespace(unsigned char value) noexcept
{
    return value == '\t' || value == '\n' || value == '\f'
        || value == '\r' || value == ' ';
}

[[nodiscard]] unsigned char ascii_lower(unsigned char value) noexcept
{
    return value >= 'A' && value <= 'Z'
        ? static_cast<unsigned char>(value - 'A' + 'a')
        : value;
}

[[nodiscard]] std::span<const std::byte> skip_ascii_whitespace(
    std::span<const std::byte> sample) noexcept
{
    std::size_t offset = 0;
    while (offset < sample.size() && ascii_whitespace(octet(sample[offset]))) {
        ++offset;
    }
    return sample.subspan(offset);
}

[[nodiscard]] bool html_prefix(
    std::span<const std::byte> sample,
    std::string_view signature,
    bool require_boundary = true) noexcept
{
    if (sample.size() < signature.size()) {
        return false;
    }
    for (std::size_t index = 0; index < signature.size(); ++index) {
        if (ascii_lower(octet(sample[index]))
            != ascii_lower(static_cast<unsigned char>(signature[index]))) {
            return false;
        }
    }
    if (!require_boundary) {
        return true;
    }
    if (sample.size() == signature.size()) {
        return false;
    }
    const unsigned char next = octet(sample[signature.size()]);
    return ascii_whitespace(next) || next == '>';
}

[[nodiscard]] bool is_html(std::span<const std::byte> sample) noexcept
{
    sample = skip_ascii_whitespace(sample);
    constexpr std::array<std::string_view, 16> tags{
        "<!DOCTYPE HTML", "<HTML", "<HEAD", "<SCRIPT", "<IFRAME", "<H1",
        "<DIV", "<FONT", "<TABLE", "<A", "<STYLE", "<TITLE", "<B",
        "<BODY", "<BR", "<P",
    };
    for (const std::string_view tag : tags) {
        if (html_prefix(sample, tag)) {
            return true;
        }
    }
    return html_prefix(sample, "<!--", false);
}

[[nodiscard]] bool riff_type(
    std::span<const std::byte> sample,
    std::string_view type) noexcept
{
    return sample.size() >= 12 && starts_with(sample, "RIFF")
        && starts_with(sample.subspan(8), type);
}

[[nodiscard]] bool is_mp4(std::span<const std::byte> sample) noexcept
{
    if (sample.size() < 12 || !starts_with(sample.subspan(4), "ftyp")) {
        return false;
    }
    constexpr std::array<std::string_view, 8> brands{
        "mp41", "mp42", "isom", "iso2", "avc1", "M4V ", "M4A ", "3gp5",
    };
    for (const std::string_view brand : brands) {
        if (starts_with(sample.subspan(8), brand)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool has_binary_control(
    std::span<const std::byte> sample) noexcept
{
    for (const std::byte value : sample) {
        const unsigned char byte = octet(value);
        if (byte <= 0x08 || byte == 0x0b
            || (byte >= 0x0e && byte <= 0x1a)
            || (byte >= 0x1c && byte <= 0x1f)) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] MimeType sniff_standard(
    std::span<const std::byte> sample) noexcept
{
    constexpr std::array<unsigned char, 4> ico{0x00, 0x00, 0x01, 0x00};
    constexpr std::array<unsigned char, 8> png{
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
    constexpr std::array<unsigned char, 3> jpeg{0xff, 0xd8, 0xff};
    constexpr std::array<unsigned char, 5> ogg{0x4f, 0x67, 0x67, 0x53, 0x00};
    constexpr std::array<unsigned char, 4> webm{0x1a, 0x45, 0xdf, 0xa3};
    constexpr std::array<unsigned char, 4> zip{0x50, 0x4b, 0x03, 0x04};
    constexpr std::array<unsigned char, 3> gzip{0x1f, 0x8b, 0x08};
    constexpr std::array<unsigned char, 4> wasm{0x00, 0x61, 0x73, 0x6d};
    constexpr std::array<unsigned char, 3> utf8_bom{0xef, 0xbb, 0xbf};
    constexpr std::array<unsigned char, 2> utf16be_bom{0xfe, 0xff};
    constexpr std::array<unsigned char, 2> utf16le_bom{0xff, 0xfe};
    constexpr std::array<unsigned char, 4> truetype{0x00, 0x01, 0x00, 0x00};

    if (starts_with(sample, ico)) {
        return MimeType::image_x_icon;
    }
    if (starts_with(sample, "BM")) {
        return MimeType::image_bmp;
    }
    if (starts_with(sample, "GIF87a") || starts_with(sample, "GIF89a")) {
        return MimeType::image_gif;
    }
    if (sample.size() >= 14 && riff_type(sample, "WEBP")
        && starts_with(sample.subspan(12), "VP")) {
        return MimeType::image_webp;
    }
    if (starts_with(sample, png)) {
        return MimeType::image_png;
    }
    if (starts_with(sample, jpeg)) {
        return MimeType::image_jpeg;
    }
    if (starts_with(sample, "%PDF-")) {
        return MimeType::application_pdf;
    }
    if (starts_with(sample, "%!PS-Adobe-")) {
        return MimeType::application_postscript;
    }
    if (starts_with(sample, "ID3")) {
        return MimeType::audio_mpeg;
    }
    if (starts_with(sample, ogg)) {
        return MimeType::application_ogg;
    }
    if (starts_with(sample, webm)) {
        return MimeType::video_webm;
    }
    if (riff_type(sample, "AVI ")) {
        return MimeType::video_avi;
    }
    if (riff_type(sample, "WAVE")) {
        return MimeType::audio_wave;
    }
    if (starts_with(sample, zip)) {
        return MimeType::application_zip;
    }
    if (starts_with(sample, gzip)) {
        return MimeType::application_x_gzip;
    }
    if (starts_with(sample, wasm)) {
        return MimeType::application_wasm;
    }
    if (is_html(sample)) {
        return MimeType::text_html_utf8;
    }
    if (starts_with(skip_ascii_whitespace(sample), "<?xml")) {
        return MimeType::text_xml_utf8;
    }
    if (starts_with(sample, utf8_bom)) {
        return MimeType::text_plain_utf8;
    }
    if (starts_with(sample, utf16be_bom)) {
        return MimeType::text_plain_utf16be;
    }
    if (starts_with(sample, utf16le_bom)) {
        return MimeType::text_plain_utf16le;
    }
    if (is_mp4(sample)) {
        return MimeType::video_mp4;
    }
    if (starts_with(sample, truetype)) {
        return MimeType::font_ttf;
    }
    if (starts_with(sample, "OTTO")) {
        return MimeType::font_otf;
    }
    if (starts_with(sample, "ttcf")) {
        return MimeType::font_collection;
    }
    if (starts_with(sample, "wOFF")) {
        return MimeType::font_woff;
    }
    if (starts_with(sample, "wOF2")) {
        return MimeType::font_woff2;
    }
    if (sample.size() >= 36 && octet(sample[34]) == 'L'
        && octet(sample[35]) == 'P') {
        return MimeType::application_eot;
    }
    return has_binary_control(sample)
        ? MimeType::application_octet_stream
        : MimeType::text_plain_utf8;
}

} // namespace

MimeType sniff_mime(std::span<const std::byte> body) noexcept
{
    const std::span<const std::byte> sample =
        body.first(std::min(body.size(), maximum_mime_sample_bytes));
    const MimeType detected = sniff_standard(sample);
    // AVIF is the sole origin-type override and is intentionally evaluated
    // only after binary fallback, at the exact major-brand offset.
    if (detected == MimeType::application_octet_stream && sample.size() >= 12
        && starts_with(sample.subspan(4), "ftypavif")) {
        return MimeType::image_avif;
    }
    return detected;
}

std::string_view mime_type_name(MimeType type) noexcept
{
    switch (type) {
    case MimeType::image_avif:
        return "image/avif";
    case MimeType::image_x_icon:
        return "image/x-icon";
    case MimeType::image_bmp:
        return "image/bmp";
    case MimeType::image_gif:
        return "image/gif";
    case MimeType::image_webp:
        return "image/webp";
    case MimeType::image_png:
        return "image/png";
    case MimeType::image_jpeg:
        return "image/jpeg";
    case MimeType::application_pdf:
        return "application/pdf";
    case MimeType::application_postscript:
        return "application/postscript";
    case MimeType::audio_mpeg:
        return "audio/mpeg";
    case MimeType::application_ogg:
        return "application/ogg";
    case MimeType::video_webm:
        return "video/webm";
    case MimeType::video_avi:
        return "video/avi";
    case MimeType::audio_wave:
        return "audio/wave";
    case MimeType::application_zip:
        return "application/zip";
    case MimeType::application_x_gzip:
        return "application/x-gzip";
    case MimeType::application_wasm:
        return "application/wasm";
    case MimeType::text_html_utf8:
        return "text/html; charset=utf-8";
    case MimeType::text_xml_utf8:
        return "text/xml; charset=utf-8";
    case MimeType::text_plain_utf8:
        return "text/plain; charset=utf-8";
    case MimeType::text_plain_utf16be:
        return "text/plain; charset=utf-16be";
    case MimeType::text_plain_utf16le:
        return "text/plain; charset=utf-16le";
    case MimeType::video_mp4:
        return "video/mp4";
    case MimeType::font_ttf:
        return "font/ttf";
    case MimeType::font_otf:
        return "font/otf";
    case MimeType::font_collection:
        return "font/collection";
    case MimeType::font_woff:
        return "font/woff";
    case MimeType::font_woff2:
        return "font/woff2";
    case MimeType::application_eot:
        return "application/vnd.ms-fontobject";
    case MimeType::application_octet_stream:
        return "application/octet-stream";
    }
    return "application/octet-stream";
}

} // namespace mediaproxy::media
