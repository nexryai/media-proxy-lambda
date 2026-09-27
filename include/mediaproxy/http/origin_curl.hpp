#pragma once

#include <cstddef>

#include <curl/curl.h>
#include <mediaproxy/http/curl_resolve_pin.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace mediaproxy::http {

inline constexpr char origin_user_agent[] = "Misskey-Media-Proxy-Go v0.10";

enum class OriginCurlConfigError {
    none,
    invalid_argument,
    curl_option,
};

[[nodiscard]] auto configure_origin_curl(CURL *easy, const OriginUrl &origin, const CurlResolvePin &pin, OriginResponseAccumulator &response, long timeout_milliseconds) noexcept -> OriginCurlConfigError;

[[nodiscard]] auto origin_header_callback(char *data, std::size_t size, std::size_t count, void *user_data) noexcept -> std::size_t;

[[nodiscard]] auto origin_body_callback(char *data, std::size_t size, std::size_t count, void *user_data) noexcept -> std::size_t;

[[nodiscard]] auto is_body_limit_completion(CURLcode result, const OriginResponseAccumulator &response) noexcept -> bool;

} // namespace mediaproxy::http
