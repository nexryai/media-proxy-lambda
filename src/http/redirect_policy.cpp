#include <mediaproxy/http/redirect_policy.hpp>

#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include <curl/curl.h>
#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

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

[[nodiscard]] bool is_ascii_alpha(char value) noexcept
{
    return (value >= 'a' && value <= 'z')
        || (value >= 'A' && value <= 'Z');
}

[[nodiscard]] bool is_ascii_digit(char value) noexcept
{
    return value >= '0' && value <= '9';
}

[[nodiscard]] bool has_scheme(std::string_view value) noexcept
{
    if (value.empty() || !is_ascii_alpha(value.front())) {
        return false;
    }
    for (std::size_t index = 1; index < value.size(); ++index) {
        const char current = value[index];
        if (current == ':') {
            return true;
        }
        if (!is_ascii_alpha(current) && !is_ascii_digit(current)
            && current != '+' && current != '-' && current != '.') {
            return false;
        }
    }
    return false;
}

[[nodiscard]] std::optional<std::string> resolve_relative_location(
    const OriginUrl& current,
    std::string_view location)
{
    CurlUrl handle{curl_url()};
    if (!handle) {
        return std::nullopt;
    }
    const std::string base{current.canonical_url};
    if (curl_url_set(handle.get(), CURLUPART_URL, base.c_str(), 0)
        != CURLUE_OK) {
        return std::nullopt;
    }
    const std::string relative{location};
    if (curl_url_set(handle.get(), CURLUPART_URL, relative.c_str(), 0)
        != CURLUE_OK) {
        return std::nullopt;
    }
    char* raw = nullptr;
    if (curl_url_get(handle.get(), CURLUPART_URL, &raw, 0) != CURLUE_OK) {
        return std::nullopt;
    }
    CurlString resolved{raw};
    return std::string{resolved.get()};
}

[[nodiscard]] bool equal_address(
    const std::optional<ValidatedAddress>& left,
    const std::optional<ValidatedAddress>& right) noexcept
{
    if (left.has_value() != right.has_value()) {
        return false;
    }
    return !left || (left->family == right->family
        && left->bytes == right->bytes);
}

[[nodiscard]] RedirectResult fail(
    RedirectError error,
    UrlError url_error = UrlError::none)
{
    return {
        .url = std::nullopt,
        .error = error,
        .url_error = url_error,
    };
}

} // namespace

RedirectTracker::RedirectTracker(OriginUrl initial)
    : current_(std::move(initial))
{
    visited_[0] = current_.canonical_url;
    visited_count_ = 1;
}

std::optional<RedirectTracker> RedirectTracker::create(
    const OriginUrl& initial)
{
    UrlPolicyResult validated =
        validate_origin_url(initial.canonical_url);
    if (!validated
        || validated.url->canonical_url != initial.canonical_url
        || validated.url->hostname != initial.hostname
        || validated.url->port != initial.port
        || validated.url->request_target != initial.request_target
        || !equal_address(
            validated.url->literal_address, initial.literal_address)) {
        return std::nullopt;
    }
    return RedirectTracker{std::move(*validated.url)};
}

RedirectResult RedirectTracker::follow(std::string_view location)
{
    if (redirect_count_ >= maximum_origin_redirects) {
        return fail(RedirectError::too_many_redirects);
    }
    if (location.find('\0') != std::string_view::npos) {
        return fail(RedirectError::invalid_location);
    }

    std::optional<std::string> resolved;
    if (has_scheme(location)) {
        resolved = std::string{location};
    } else {
        resolved = resolve_relative_location(current_, location);
    }
    if (!resolved) {
        return fail(RedirectError::invalid_location);
    }

    UrlPolicyResult validated = validate_origin_url(*resolved);
    if (!validated) {
        return fail(RedirectError::url_policy, validated.error);
    }
    for (std::size_t index = 0; index < visited_count_; ++index) {
        if (visited_[index] == validated.url->canonical_url) {
            return fail(RedirectError::loop);
        }
    }

    static_assert(std::is_nothrow_move_assignable_v<OriginUrl>);
    OriginUrl accepted = std::move(*validated.url);
    OriginUrl returned = accepted;
    std::string history = accepted.canonical_url;
    current_ = std::move(accepted);
    visited_[visited_count_] = std::move(history);
    ++visited_count_;
    ++redirect_count_;
    return {
        .url = std::move(returned),
        .error = RedirectError::none,
        .url_error = UrlError::none,
    };
}

const OriginUrl& RedirectTracker::current() const noexcept
{
    return current_;
}

std::size_t RedirectTracker::redirect_count() const noexcept
{
    return redirect_count_;
}

} // namespace mediaproxy::http
