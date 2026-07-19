#include <mediaproxy/http/origin_download.hpp>

#include <memory>
#include <optional>
#include <utility>

#include <curl/curl.h>
#include <mediaproxy/http/curl_resolve_pin.hpp>
#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/origin_curl.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/redirect_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace mediaproxy::http {
namespace {

CURL* create_system_easy(void*)
{
    return curl_easy_init();
}

void destroy_system_easy(CURL* easy, void*)
{
    curl_easy_cleanup(easy);
}

CURLcode perform_system_request(
    CURL* easy,
    OriginResponseAccumulator&,
    void*)
{
    return curl_easy_perform(easy);
}

CURLcode read_system_response_code(CURL* easy, long* status, void*)
{
    return curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, status);
}

class EasyDeleter final {
public:
    explicit EasyDeleter(OriginTransportApi transport) noexcept
        : transport_(transport)
    {
    }

    void operator()(CURL* easy) const noexcept
    {
        if (easy != nullptr && transport_.destroy != nullptr) {
            transport_.destroy(easy, transport_.context);
        }
    }

private:
    OriginTransportApi transport_;
};

using EasyHandle = std::unique_ptr<CURL, EasyDeleter>;

[[nodiscard]] bool valid_transport(
    const OriginTransportApi& transport) noexcept
{
    return transport.create != nullptr && transport.destroy != nullptr
        && transport.perform != nullptr
        && transport.response_code != nullptr;
}

[[nodiscard]] bool valid_timeout(const OriginTimeoutApi& timeout) noexcept
{
    return timeout.remaining_milliseconds != nullptr;
}

[[nodiscard]] bool is_redirect_status(long status) noexcept
{
    return status == 301 || status == 302 || status == 303 || status == 307
        || status == 308;
}

[[nodiscard]] OriginDownloadResult perform_origin_request(
    const OriginUrl& origin,
    OriginTimeoutApi timeout,
    AddressResolverApi resolver,
    OriginTransportApi transport)
{
    OriginDownloadResult result;
    if (!valid_transport(transport) || !valid_timeout(timeout)) {
        result.error = OriginDownloadError::invalid_argument;
        return result;
    }
    if (timeout.remaining_milliseconds(timeout.context) <= 0) {
        result.error = OriginDownloadError::deadline;
        return result;
    }

    // Resolution is the SSRF boundary: no origin transfer handle is created
    // until every returned address has passed the public-address policy.
    result.resolution = resolve_origin_addresses(origin, resolver);
    if (!result.resolution) {
        result.error = OriginDownloadError::resolution;
        return result;
    }

    CurlResolvePin pin =
        CurlResolvePin::create(origin, result.resolution.addresses);
    result.pin_error = pin.error();
    if (!pin) {
        result.error = OriginDownloadError::resolve_pin;
        return result;
    }

    // DNS can consume part of the invocation budget. Refresh the remaining
    // time before giving curl its connect and whole-transfer limits.
    const long timeout_milliseconds =
        timeout.remaining_milliseconds(timeout.context);
    if (timeout_milliseconds <= 0) {
        result.error = OriginDownloadError::deadline;
        return result;
    }

    EasyHandle easy{
        transport.create(transport.context), EasyDeleter{transport}};
    if (!easy) {
        result.error = OriginDownloadError::easy_init;
        return result;
    }

    result.config_error = configure_origin_curl(
        easy.get(), origin, pin, result.response, timeout_milliseconds);
    if (result.config_error != OriginCurlConfigError::none) {
        result.error = OriginDownloadError::curl_config;
        return result;
    }

    result.curl_error =
        transport.perform(easy.get(), result.response, transport.context);
    // The compatibility contract retains exactly 10 MiB without reading a
    // probe byte. curl reports the callback's intentional short write as an
    // error, but this one fully classified condition is a completed body.
    if (result.curl_error != CURLE_OK
        && !is_body_limit_completion(result.curl_error, result.response)) {
        result.error = result.response.error() == OriginResponseError::none
            ? OriginDownloadError::transfer
            : OriginDownloadError::response_policy;
        return result;
    }

    result.curl_error = transport.response_code(
        easy.get(), &result.status, transport.context);
    if (result.curl_error != CURLE_OK) {
        result.error = OriginDownloadError::response_info;
        return result;
    }
    if (result.response.error() != OriginResponseError::none) {
        result.error = OriginDownloadError::response_policy;
    }
    return result;
}

struct FixedTimeout {
    long milliseconds = 0;
};

long fixed_remaining_time(void* context)
{
    return static_cast<FixedTimeout*>(context)->milliseconds;
}

} // namespace

OriginTransportApi system_origin_transport() noexcept
{
    return {
        .context = nullptr,
        .create = &create_system_easy,
        .destroy = &destroy_system_easy,
        .perform = &perform_system_request,
        .response_code = &read_system_response_code,
    };
}

OriginDownloadResult download_origin_once(
    const OriginUrl& origin,
    long timeout_milliseconds,
    AddressResolverApi resolver,
    OriginTransportApi transport)
{
    if (timeout_milliseconds <= 0) {
        OriginDownloadResult result;
        result.error = OriginDownloadError::invalid_argument;
        return result;
    }
    FixedTimeout fixed{.milliseconds = timeout_milliseconds};
    OriginDownloadResult result = perform_origin_request(
        origin,
        {.context = &fixed, .remaining_milliseconds = &fixed_remaining_time},
        resolver,
        transport);
    if (!result) {
        return result;
    }
    if (!result.response.finish(result.status)) {
        result.error = OriginDownloadError::response_policy;
    }
    return result;
}

OriginDownloadResult download_origin(
    const OriginUrl& initial,
    OriginTimeoutApi timeout,
    AddressResolverApi resolver,
    OriginTransportApi transport)
{
    OriginDownloadResult result;
    if (!valid_transport(transport) || !valid_timeout(timeout)) {
        result.error = OriginDownloadError::invalid_argument;
        return result;
    }

    std::optional<RedirectTracker> tracker = RedirectTracker::create(initial);
    if (!tracker) {
        result.error = OriginDownloadError::resolution;
        result.resolution.error = OriginResolutionError::invalid_origin;
        return result;
    }

    while (true) {
        // A new handle and resolve pin are created for every hop so no prior
        // hostname's validated address set can leak into the next request.
        result = perform_origin_request(
            tracker->current(), timeout, resolver, transport);
        result.redirect_count = tracker->redirect_count();
        if (!result) {
            return result;
        }
        if (result.status == 200) {
            if (!result.response.finish(result.status)) {
                result.error = OriginDownloadError::response_policy;
            }
            return result;
        }
        if (!is_redirect_status(result.status)) {
            if (!result.response.finish(result.status)) {
                result.error = OriginDownloadError::response_policy;
            }
            return result;
        }

        const auto& location = result.response.location();
        if (!location || location->empty()) {
            result.error = OriginDownloadError::redirect;
            result.redirect_error = RedirectError::invalid_location;
            return result;
        }
        RedirectResult redirected = tracker->follow(*location);
        if (!redirected) {
            result.error = OriginDownloadError::redirect;
            result.redirect_error = redirected.error;
            result.redirect_url_error = redirected.url_error;
            return result;
        }
    }
}

} // namespace mediaproxy::http
