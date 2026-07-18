#include <mediaproxy/http/url_policy.hpp>

#include <charconv>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include <curl/curl.h>
#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/idna.hpp>

namespace mediaproxy::http {
namespace {

struct CurlUrlDeleter {
    void operator()(CURLU* handle) const noexcept
    {
        curl_url_cleanup(handle);
    }
};

struct CurlStringDeleter {
    void operator()(char* value) const noexcept
    {
        curl_free(value);
    }
};

using CurlUrl = std::unique_ptr<CURLU, CurlUrlDeleter>;
using CurlString = std::unique_ptr<char, CurlStringDeleter>;

[[nodiscard]] UrlPolicyResult fail(UrlError error)
{
    return {.url = std::nullopt, .error = error};
}

[[nodiscard]] std::optional<std::string> get_part(
    CURLU* handle,
    CURLUPart part)
{
    char* raw = nullptr;
    if (curl_url_get(handle, part, &raw, 0) != CURLUE_OK) {
        return std::nullopt;
    }
    CurlString value{raw};
    return std::string{value.get()};
}

[[nodiscard]] bool has_user_information(CURLU* handle) noexcept
{
    char* raw = nullptr;
    const CURLUcode user_result =
        curl_url_get(handle, CURLUPART_USER, &raw, 0);
    CurlString user{raw};
    raw = nullptr;
    const CURLUcode password_result =
        curl_url_get(handle, CURLUPART_PASSWORD, &raw, 0);
    CurlString password{raw};
    return user_result != CURLUE_NO_USER || password_result != CURLUE_NO_PASSWORD;
}

[[nodiscard]] std::optional<std::uint16_t> parse_port(CURLU* handle)
{
    const auto value = get_part(handle, CURLUPART_PORT);
    if (!value) {
        return std::uint16_t{443};
    }
    unsigned int parsed = 0;
    const auto result = std::from_chars(
        value->data(), value->data() + value->size(), parsed, 10);
    if (result.ec != std::errc{} || result.ptr != value->data() + value->size()
        || (parsed != 80 && parsed != 443)) {
        return std::nullopt;
    }
    return static_cast<std::uint16_t>(parsed);
}

} // namespace

UrlPolicyResult validate_origin_url(std::string_view source)
{
    if (source.find('\0') != std::string_view::npos) {
        return fail(UrlError::invalid_syntax);
    }
    if (!source.starts_with("https://")) {
        return fail(UrlError::invalid_scheme);
    }
    constexpr std::size_t authority_start = sizeof("https://") - 1;
    const std::size_t authority_end = source.find_first_of("/?#", authority_start);
    if (authority_end == authority_start) {
        return fail(UrlError::invalid_hostname);
    }

    CurlUrl handle{curl_url()};
    if (!handle) {
        return fail(UrlError::invalid_syntax);
    }
    const std::string terminated{source};
    if (curl_url_set(handle.get(), CURLUPART_URL, terminated.c_str(), 0)
        != CURLUE_OK) {
        return fail(UrlError::invalid_syntax);
    }
    if (has_user_information(handle.get())) {
        return fail(UrlError::user_information);
    }

    const auto raw_hostname = get_part(handle.get(), CURLUPART_HOST);
    if (!raw_hostname || raw_hostname->empty() || raw_hostname->find(':') != std::string::npos) {
        return fail(UrlError::invalid_hostname);
    }
    auto normalized = normalize_hostname(*raw_hostname);
    if (!normalized || normalized.ascii.find('.') == std::string::npos) {
        return fail(UrlError::invalid_hostname);
    }

    const auto port = parse_port(handle.get());
    if (!port) {
        return fail(UrlError::invalid_port);
    }
    const auto path = get_part(handle.get(), CURLUPART_PATH);
    if (!path) {
        return fail(UrlError::invalid_syntax);
    }
    std::string request_target = path->empty() ? "/" : *path;
    if (const auto query = get_part(handle.get(), CURLUPART_QUERY)) {
        request_target.push_back('?');
        request_target.append(*query);
    }

    std::optional<ValidatedAddress> literal_address;
    const auto address = validate_public_address(normalized.ascii);
    if (address) {
        literal_address = address.address;
    } else if (address.error != AddressError::parse_failure) {
        return fail(UrlError::forbidden_address);
    }

    std::string canonical_url = "https://" + normalized.ascii;
    if (*port != 443) {
        canonical_url.append(":80");
    }
    canonical_url.append(request_target);
    return {
        .url = OriginUrl{
            .canonical_url = std::move(canonical_url),
            .hostname = std::move(normalized.ascii),
            .port = *port,
            .request_target = std::move(request_target),
            .literal_address = std::move(literal_address),
        },
        .error = UrlError::none,
    };
}

} // namespace mediaproxy::http
