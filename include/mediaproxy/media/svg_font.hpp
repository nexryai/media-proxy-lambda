#pragma once

#include <cstddef>
#include <span>

namespace mediaproxy::media {

[[nodiscard]] auto embedded_svg_font() noexcept -> std::span<const std::byte>;

} // namespace mediaproxy::media
