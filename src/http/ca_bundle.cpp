#include <mediaproxy/http/ca_bundle.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>

extern "C" {

// llvm-objcopy exports these exact names; keep them as asm labels while the
// C++ identifiers follow the project's naming rule.
extern const unsigned char binary_cacert_pem_start[] asm("_binary_cacert_pem_start");
extern const unsigned char binary_cacert_pem_end[] asm("_binary_cacert_pem_end");
}

namespace mediaproxy::http {

auto embedded_ca_bundle() noexcept -> std::span<const std::byte> {
    const auto *const begin = reinterpret_cast<const std::byte *>(binary_cacert_pem_start);
    const auto begin_address = reinterpret_cast<std::uintptr_t>(begin);
    const auto end_address = reinterpret_cast<std::uintptr_t>(binary_cacert_pem_end);
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
