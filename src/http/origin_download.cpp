#include <mediaproxy/http/origin_download.hpp>

#include <memory>
#include <utility>

#include <curl/curl.h>
#include <mediaproxy/http/curl_resolve_pin.hpp>
#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/origin_curl.hpp>
#include <mediaproxy/http/origin_response.hpp>
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
    OriginDownloadResult result;
    if (timeout_milliseconds <= 0 || !valid_transport(transport)) {
        result.error = OriginDownloadError::invalid_argument;
        return result;
    }

    // Resolution is the SSRF boundary: no socket-capable object is created
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
    if (!result.response.finish(result.status)) {
        result.error = OriginDownloadError::response_policy;
        return result;
    }

    return result;
}

} // namespace mediaproxy::http
