#include <mediaproxy/http/address_policy.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <arpa/inet.h>

namespace mediaproxy::http {
namespace {

using AddressBytes = std::array<std::uint8_t, 16>;

[[nodiscard]] AddressPolicyResult fail(AddressError error)
{
    return {.address = {}, .error = error};
}

template <std::size_t Size>
[[nodiscard]] bool has_prefix(
    const AddressBytes& address,
    const std::array<std::uint8_t, Size>& prefix,
    unsigned int prefix_bits) noexcept
{
    const std::size_t full_bytes = prefix_bits / 8U;
    const unsigned int remaining_bits = prefix_bits % 8U;
    if (!std::equal(prefix.begin(), prefix.begin() + full_bytes, address.begin())) {
        return false;
    }
    if (remaining_bits == 0) {
        return true;
    }
    const auto mask = static_cast<std::uint8_t>(0xffU << (8U - remaining_bits));
    return (address[full_bytes] & mask) == (prefix[full_bytes] & mask);
}

[[nodiscard]] bool ipv4_prefix(
    const AddressBytes& address,
    std::array<std::uint8_t, 4> prefix,
    unsigned int prefix_bits) noexcept
{
    return has_prefix(address, prefix, prefix_bits);
}

[[nodiscard]] bool ipv6_prefix(
    const AddressBytes& address,
    std::array<std::uint8_t, 16> prefix,
    unsigned int prefix_bits) noexcept
{
    return has_prefix(address, prefix, prefix_bits);
}

[[nodiscard]] AddressError classify_ipv4(const AddressBytes& address) noexcept
{
    if (ipv4_prefix(address, {0, 0, 0, 0}, 32)) {
        return AddressError::unspecified;
    }
    if (ipv4_prefix(address, {127, 0, 0, 0}, 8)) {
        return AddressError::loopback;
    }
    if (ipv4_prefix(address, {10, 0, 0, 0}, 8)
        || ipv4_prefix(address, {172, 16, 0, 0}, 12)
        || ipv4_prefix(address, {192, 168, 0, 0}, 16)) {
        return AddressError::private_address;
    }
    if (ipv4_prefix(address, {169, 254, 0, 0}, 16)) {
        return AddressError::link_local;
    }
    if (ipv4_prefix(address, {224, 0, 0, 0}, 4)) {
        return AddressError::multicast;
    }
    if (ipv4_prefix(address, {0, 0, 0, 0}, 8)
        || ipv4_prefix(address, {100, 64, 0, 0}, 10)) {
        return AddressError::denied_range;
    }

    if (ipv4_prefix(address, {192, 0, 0, 0}, 24)
        || ipv4_prefix(address, {192, 0, 2, 0}, 24)
        || ipv4_prefix(address, {192, 88, 99, 0}, 24)
        || ipv4_prefix(address, {198, 18, 0, 0}, 15)
        || ipv4_prefix(address, {198, 51, 100, 0}, 24)
        || ipv4_prefix(address, {203, 0, 113, 0}, 24)
        || ipv4_prefix(address, {240, 0, 0, 0}, 4)) {
        return AddressError::not_global_unicast;
    }
    return AddressError::none;
}

[[nodiscard]] AddressError classify_ipv6(const AddressBytes& address) noexcept
{
    constexpr std::array<std::uint8_t, 16> zero{};
    if (address == zero) {
        return AddressError::unspecified;
    }
    auto loopback = zero;
    loopback.back() = 1;
    if (address == loopback) {
        return AddressError::loopback;
    }
    if (ipv6_prefix(address, {0, 0, 0, 0, 0, 0, 0, 0,
                              0, 0, 0xff, 0xff, 0, 0, 0, 0}, 96)) {
        return AddressError::mapped_ipv6;
    }
    if (ipv6_prefix(address, {0xfc}, 7)) {
        return AddressError::private_address;
    }
    if (ipv6_prefix(address, {0xfe, 0x80}, 10)) {
        return AddressError::link_local;
    }
    if (ipv6_prefix(address, {0xff}, 8)) {
        return AddressError::multicast;
    }

    if (ipv6_prefix(address, {}, 96)
        || ipv6_prefix(address, {0x00, 0x64, 0xff, 0x9b}, 96)
        || ipv6_prefix(address, {0x00, 0x64, 0xff, 0x9b, 0x00, 0x01}, 48)
        || ipv6_prefix(address, {0x20, 0x01, 0x00, 0x10}, 28)
        || ipv6_prefix(address, {0x20, 0x01, 0x0d, 0xb8}, 32)) {
        return AddressError::denied_range;
    }

    if (ipv6_prefix(address, {0x01, 0x00}, 64)
        || ipv6_prefix(address, {0x20, 0x01}, 23)
        || ipv6_prefix(address, {0x20, 0x02}, 16)
        || ipv6_prefix(address, {0x3f, 0xff}, 20)
        || !ipv6_prefix(address, {0x20}, 3)) {
        return AddressError::not_global_unicast;
    }
    return AddressError::none;
}

} // namespace

AddressPolicyResult validate_public_address(std::string_view text)
{
    if (text.empty() || text.size() >= INET6_ADDRSTRLEN
        || text.find('\0') != std::string_view::npos) {
        return fail(AddressError::parse_failure);
    }
    if (text.starts_with("::ffff:") || text.starts_with("::ffff:0:")) {
        return fail(AddressError::mapped_ipv6);
    }

    const std::string terminated{text};
    AddressBytes bytes{};
    if (inet_pton(AF_INET, terminated.c_str(), bytes.data()) == 1) {
        const AddressError error = classify_ipv4(bytes);
        return error == AddressError::none
            ? AddressPolicyResult{
                  .address = {.family = AddressFamily::ipv4, .bytes = bytes},
                  .error = AddressError::none}
            : fail(error);
    }

    bytes = {};
    if (inet_pton(AF_INET6, terminated.c_str(), bytes.data()) == 1) {
        const AddressError error = classify_ipv6(bytes);
        return error == AddressError::none
            ? AddressPolicyResult{
                  .address = {.family = AddressFamily::ipv6, .bytes = bytes},
                  .error = AddressError::none}
            : fail(error);
    }
    return fail(AddressError::parse_failure);
}

} // namespace mediaproxy::http
