#pragma once

#include <cstddef>
#include <span>

namespace mediaproxy::media {

[[nodiscard]] std::span<const std::byte> embedded_svg_font() noexcept;

} // namespace mediaproxy::media
