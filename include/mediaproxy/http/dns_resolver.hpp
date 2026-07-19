#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include <netdb.h>
#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/dns_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace mediaproxy::http {

inline constexpr std::size_t maximum_dns_candidates = 64;

using AddressLookupFunction = int (*)(
    const char*,
    const char*,
    const addrinfo*,
    addrinfo**);
using AddressReleaseFunction = void (*)(addrinfo*);

struct AddressResolverApi {
    AddressLookupFunction lookup = &getaddrinfo;
    AddressReleaseFunction release = &freeaddrinfo;
};

enum class OriginResolutionError {
    none,
    invalid_origin,
    lookup_failure,
    empty_answer,
    malformed_answer,
    too_many_answers,
    address_policy,
};

struct OriginResolutionResult {
    std::vector<ValidatedAddress> addresses;
    OriginResolutionError error = OriginResolutionError::none;
    int native_error = 0;
    ResolutionError policy_error = ResolutionError::none;
    std::optional<std::size_t> rejected_index;
    AddressError address_error = AddressError::none;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == OriginResolutionError::none;
    }
};

[[nodiscard]] OriginResolutionResult resolve_origin_addresses(
    const OriginUrl& origin,
    AddressResolverApi api = {});

} // namespace mediaproxy::http
