#include <mediaproxy/media/svg_font.hpp>

#include <cstddef>

extern "C" {
extern const unsigned char _binary_mediaproxy_svg_font_ttf_start[];
extern const unsigned char _binary_mediaproxy_svg_font_ttf_end[];
}

namespace mediaproxy::media {

std::span<const std::byte> embedded_svg_font() noexcept
{
    const auto* begin = reinterpret_cast<const std::byte*>(
        _binary_mediaproxy_svg_font_ttf_start);
    const auto* end = reinterpret_cast<const std::byte*>(
        _binary_mediaproxy_svg_font_ttf_end);
    return {begin, static_cast<std::size_t>(end - begin)};
}

} // namespace mediaproxy::media
