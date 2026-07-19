#pragma once

#include <cstddef>

#include <curl/curl.h>
#include <mediaproxy/http/curl_resolve_pin.hpp>
#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/origin_curl.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/redirect_policy.hpp>
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

using OriginRemainingTimeFunction = long (*)(void* context);

struct OriginTimeoutApi {
    // The caller subtracts the response-submission reserve before returning.
    void* context = nullptr;
    OriginRemainingTimeFunction remaining_milliseconds = nullptr;
};

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
    deadline,
    redirect,
};

struct OriginDownloadResult {
    OriginResponseAccumulator response;
    OriginResolutionResult resolution;
    OriginDownloadError error = OriginDownloadError::none;
    ResolvePinError pin_error = ResolvePinError::none;
    OriginCurlConfigError config_error = OriginCurlConfigError::none;
    RedirectError redirect_error = RedirectError::none;
    UrlError redirect_url_error = UrlError::none;
    CURLcode curl_error = CURLE_OK;
    long status = 0;
    std::size_t redirect_count = 0;

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

[[nodiscard]] OriginDownloadResult download_origin(
    const OriginUrl& initial,
    OriginTimeoutApi timeout,
    AddressResolverApi resolver = {},
    OriginTransportApi transport = system_origin_transport());

} // namespace mediaproxy::http
