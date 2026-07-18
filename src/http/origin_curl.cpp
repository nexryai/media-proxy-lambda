#include <mediaproxy/http/origin_curl.hpp>

#include <cstddef>
#include <limits>
#include <span>
#include <string_view>

#include <curl/curl.h>
#include <mediaproxy/http/curl_resolve_pin.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace mediaproxy::http {
namespace {

template <typename Value>
[[nodiscard]] bool set_option(
    CURL* easy,
    CURLoption option,
    Value value) noexcept
{
    return curl_easy_setopt(easy, option, value) == CURLE_OK;
}

[[nodiscard]] bool callback_size(
    std::size_t size,
    std::size_t count,
    std::size_t& total) noexcept
{
    if (size != 0 && count > std::numeric_limits<std::size_t>::max() / size) {
        return false;
    }
    total = size * count;
    return true;
}

} // namespace

OriginCurlConfigError configure_origin_curl(
    CURL* easy,
    const OriginUrl& origin,
    const CurlResolvePin& pin,
    OriginResponseAccumulator& response,
    std::span<const std::byte> ca_pem,
    long timeout_milliseconds) noexcept
{
    if (easy == nullptr || !pin.matches(origin) || ca_pem.empty()
        || timeout_milliseconds <= 0) {
        return OriginCurlConfigError::invalid_argument;
    }

    curl_blob ca_blob{
        .data = const_cast<std::byte*>(ca_pem.data()),
        .len = ca_pem.size(),
        .flags = CURL_BLOB_COPY,
    };
    const bool configured =
        set_option(easy, CURLOPT_URL, origin.canonical_url.c_str())
        && set_option(easy, CURLOPT_RESOLVE, pin.native_handle())
        && set_option(easy, CURLOPT_HTTPGET, 1L)
        && set_option(easy, CURLOPT_USERAGENT, origin_user_agent)
        && set_option(easy, CURLOPT_ACCEPT_ENCODING, "")
        && set_option(easy, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS)
        && set_option(easy, CURLOPT_PROTOCOLS_STR, "https")
        && set_option(easy, CURLOPT_REDIR_PROTOCOLS_STR, "https")
        && set_option(easy, CURLOPT_FOLLOWLOCATION, 0L)
        && set_option(easy, CURLOPT_SSL_VERIFYPEER, 1L)
        && set_option(easy, CURLOPT_SSL_VERIFYHOST, 2L)
        && set_option(easy, CURLOPT_CAINFO_BLOB, &ca_blob)
        && set_option(easy, CURLOPT_NOSIGNAL, 1L)
        && set_option(easy, CURLOPT_CONNECTTIMEOUT_MS, timeout_milliseconds)
        && set_option(easy, CURLOPT_TIMEOUT_MS, timeout_milliseconds)
        && set_option(easy, CURLOPT_FAILONERROR, 0L)
        && set_option(easy, CURLOPT_HEADERFUNCTION, origin_header_callback)
        && set_option(easy, CURLOPT_HEADERDATA, &response)
        && set_option(easy, CURLOPT_WRITEFUNCTION, origin_body_callback)
        && set_option(easy, CURLOPT_WRITEDATA, &response);
    return configured
        ? OriginCurlConfigError::none
        : OriginCurlConfigError::curl_option;
}

std::size_t origin_header_callback(
    char* data,
    std::size_t size,
    std::size_t count,
    void* user_data) noexcept
{
    std::size_t total = 0;
    if (data == nullptr || user_data == nullptr
        || !callback_size(size, count, total)) {
        return 0;
    }
    auto& response = *static_cast<OriginResponseAccumulator*>(user_data);
    response.consume_header_line(std::string_view{data, total});
    return response.error() == OriginResponseError::none ? total : 0;
}

std::size_t origin_body_callback(
    char* data,
    std::size_t size,
    std::size_t count,
    void* user_data) noexcept
{
    std::size_t total = 0;
    if (data == nullptr || user_data == nullptr
        || !callback_size(size, count, total)) {
        return 0;
    }
    auto& response = *static_cast<OriginResponseAccumulator*>(user_data);
    const auto* bytes = reinterpret_cast<const std::byte*>(data);
    return response.append_body(std::span{bytes, total});
}

bool is_body_limit_completion(
    CURLcode result,
    const OriginResponseAccumulator& response) noexcept
{
    return result == CURLE_WRITE_ERROR
        && response.error() == OriginResponseError::none
        && response.at_body_limit();
}

} // namespace mediaproxy::http
