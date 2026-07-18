#pragma once

#include <cstddef>
#include <span>

namespace mediaproxy::http {

[[nodiscard]] std::span<const std::byte> embedded_ca_bundle() noexcept;

} // namespace mediaproxy::http
