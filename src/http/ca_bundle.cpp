#include <mediaproxy/http/ca_bundle.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

extern "C" {

extern const unsigned char _binary_cacert_pem_start[];
extern const unsigned char _binary_cacert_pem_end[];
}

namespace mediaproxy::http {

std::span<const std::byte> embedded_ca_bundle() noexcept {
    const auto *const begin =
        reinterpret_cast<const std::byte *>(_binary_cacert_pem_start);
    const auto begin_address = reinterpret_cast<std::uintptr_t>(begin);
    const auto end_address =
        reinterpret_cast<std::uintptr_t>(_binary_cacert_pem_end);
    if (end_address < begin_address) {
        return {};
    }
    const auto byte_count = end_address - begin_address;
    if (byte_count > std::numeric_limits<std::size_t>::max()) {
        return {};
    }
    return {begin, static_cast<std::size_t>(byte_count)};
}

} // namespace mediaproxy::http
