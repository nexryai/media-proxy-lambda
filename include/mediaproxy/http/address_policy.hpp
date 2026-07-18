#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace mediaproxy::http {

enum class AddressFamily {
    ipv4,
    ipv6,
};

enum class AddressError {
    none,
    parse_failure,
    mapped_ipv6,
    loopback,
    private_address,
    link_local,
    multicast,
    unspecified,
    denied_range,
    not_global_unicast,
};

struct ValidatedAddress {
    AddressFamily family = AddressFamily::ipv4;
    std::array<std::uint8_t, 16> bytes{};
};

struct AddressPolicyResult {
    ValidatedAddress address;
    AddressError error = AddressError::none;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == AddressError::none;
    }
};

[[nodiscard]] AddressPolicyResult validate_public_address(
    std::string_view text);

} // namespace mediaproxy::http
