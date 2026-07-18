#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

#include <mediaproxy/http/url_policy.hpp>

namespace mediaproxy::http {

inline constexpr std::size_t maximum_origin_redirects = 10;

enum class RedirectError {
    none,
    invalid_location,
    url_policy,
    loop,
    too_many_redirects,
};

struct RedirectResult {
    std::optional<OriginUrl> url;
    RedirectError error = RedirectError::none;
    UrlError url_error = UrlError::none;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return url.has_value();
    }
};

class RedirectTracker final {
public:
    [[nodiscard]] static std::optional<RedirectTracker> create(
        const OriginUrl& initial);

    [[nodiscard]] RedirectResult follow(std::string_view location);

    [[nodiscard]] const OriginUrl& current() const noexcept;
    [[nodiscard]] std::size_t redirect_count() const noexcept;

private:
    explicit RedirectTracker(OriginUrl initial);

    OriginUrl current_;
    std::array<std::string, maximum_origin_redirects + 1> visited_{};
    std::size_t visited_count_ = 0;
    std::size_t redirect_count_ = 0;
};

} // namespace mediaproxy::http
