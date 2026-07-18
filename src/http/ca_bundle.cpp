#include <mediaproxy/http/ca_bundle.hpp>

#include <cstddef>
#include <span>

extern "C" {

extern const unsigned char _binary_cacert_pem_start[];
extern const unsigned char _binary_cacert_pem_end[];

}

namespace mediaproxy::http {

std::span<const std::byte> embedded_ca_bundle() noexcept
{
    const auto* const begin =
        reinterpret_cast<const std::byte*>(_binary_cacert_pem_start);
    const auto* const end =
        reinterpret_cast<const std::byte*>(_binary_cacert_pem_end);
    return {begin, static_cast<std::size_t>(end - begin)};
}

} // namespace mediaproxy::http
