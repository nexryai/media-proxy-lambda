#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include <mediaproxy/http/address_policy.hpp>

namespace mediaproxy::http {

enum class UrlError {
    none,
    invalid_syntax,
    invalid_scheme,
    user_information,
    invalid_hostname,
    invalid_port,
    forbidden_address,
};

struct OriginUrl {
    std::string canonical_url;
    std::string hostname;
    std::uint16_t port = 443;
    std::string request_target;
    std::optional<ValidatedAddress> literal_address;
};

struct UrlPolicyResult {
    std::optional<OriginUrl> url;
    UrlError error = UrlError::none;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return url.has_value();
    }
};

[[nodiscard]] UrlPolicyResult validate_origin_url(std::string_view source);

} // namespace mediaproxy::http
