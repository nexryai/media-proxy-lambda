#pragma once

#include <cstddef>
#include <span>

#include <curl/curl.h>
#include <mediaproxy/http/curl_resolve_pin.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace mediaproxy::http {

inline constexpr char origin_user_agent[] =
    "Misskey-Media-Proxy-Go v0.10";

enum class OriginCurlConfigError {
    none,
    invalid_argument,
    curl_option,
};

[[nodiscard]] OriginCurlConfigError configure_origin_curl(
    CURL* easy,
    const OriginUrl& origin,
    const CurlResolvePin& pin,
    OriginResponseAccumulator& response,
    std::span<const std::byte> ca_pem,
    long timeout_milliseconds) noexcept;

[[nodiscard]] std::size_t origin_header_callback(
    char* data,
    std::size_t size,
    std::size_t count,
    void* user_data) noexcept;

[[nodiscard]] std::size_t origin_body_callback(
    char* data,
    std::size_t size,
    std::size_t count,
    void* user_data) noexcept;

[[nodiscard]] bool is_body_limit_completion(
    CURLcode result,
    const OriginResponseAccumulator& response) noexcept;

} // namespace mediaproxy::http
