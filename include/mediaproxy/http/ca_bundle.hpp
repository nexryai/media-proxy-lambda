#pragma once

#include <cstddef>
#include <span>

namespace mediaproxy::http {

[[nodiscard]] auto embedded_ca_bundle() noexcept -> std::span<const std::byte>;

} // namespace mediaproxy::http
