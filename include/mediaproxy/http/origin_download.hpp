#pragma once

#include <curl/curl.h>
#include <mediaproxy/http/curl_resolve_pin.hpp>
#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/origin_curl.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace mediaproxy::http {

using OriginEasyCreateFunction = CURL* (*)(void* context);
using OriginEasyDestroyFunction = void (*)(CURL* easy, void* context);
using OriginPerformFunction = CURLcode (*)(
    CURL* easy,
    OriginResponseAccumulator& response,
    void* context);
using OriginResponseCodeFunction = CURLcode (*)(
    CURL* easy,
    long* status,
    void* context);

struct OriginTransportApi {
    void* context = nullptr;
    OriginEasyCreateFunction create = nullptr;
    OriginEasyDestroyFunction destroy = nullptr;
    OriginPerformFunction perform = nullptr;
    OriginResponseCodeFunction response_code = nullptr;
};

[[nodiscard]] OriginTransportApi system_origin_transport() noexcept;

enum class OriginDownloadError {
    none,
    invalid_argument,
    resolution,
    resolve_pin,
    easy_init,
    curl_config,
    transfer,
    response_info,
    response_policy,
};

struct OriginDownloadResult {
    OriginResponseAccumulator response;
    OriginResolutionResult resolution;
    OriginDownloadError error = OriginDownloadError::none;
    ResolvePinError pin_error = ResolvePinError::none;
    OriginCurlConfigError config_error = OriginCurlConfigError::none;
    CURLcode curl_error = CURLE_OK;
    long status = 0;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == OriginDownloadError::none;
    }
};

[[nodiscard]] OriginDownloadResult download_origin_once(
    const OriginUrl& origin,
    long timeout_milliseconds,
    AddressResolverApi resolver = {},
    OriginTransportApi transport = system_origin_transport());

} // namespace mediaproxy::http
