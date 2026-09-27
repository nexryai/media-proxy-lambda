#include <mediaproxy/media/svg_font.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>

extern "C" {
// llvm-objcopy exports these exact names; keep them as asm labels while the
// C++ identifiers follow the project's naming rule.
extern const unsigned char binary_mediaproxy_svg_font_ttf_start[] asm("_binary_mediaproxy_svg_font_ttf_start");
extern const unsigned char binary_mediaproxy_svg_font_ttf_end[] asm("_binary_mediaproxy_svg_font_ttf_end");
}

namespace mediaproxy::media {

auto embedded_svg_font() noexcept -> std::span<const std::byte> {
    const auto *begin = reinterpret_cast<const std::byte *>(binary_mediaproxy_svg_font_ttf_start);
    const auto begin_address = reinterpret_cast<std::uintptr_t>(begin);
    const auto end_address = reinterpret_cast<std::uintptr_t>(binary_mediaproxy_svg_font_ttf_end);
    if (end_address < begin_address) {
        return {};
    }
    const auto byte_count = end_address - begin_address;
    if (byte_count > std::numeric_limits<std::size_t>::max()) {
        return {};
    }

    return {begin, static_cast<std::size_t>(byte_count)};
}

} // namespace mediaproxy::media
