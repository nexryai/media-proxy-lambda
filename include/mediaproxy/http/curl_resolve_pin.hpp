#pragma once

#include <cstdint>
#include <span>
#include <string>

#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

struct curl_slist;

namespace mediaproxy::http {

enum class ResolvePinError {
    none,
    invalid_origin,
    empty_addresses,
    address_format,
    allocation,
};

class CurlResolvePin final {
public:
    CurlResolvePin() noexcept = default;
    ~CurlResolvePin();

    CurlResolvePin(const CurlResolvePin&) = delete;
    CurlResolvePin& operator=(const CurlResolvePin&) = delete;
    CurlResolvePin(CurlResolvePin&& other) noexcept;
    CurlResolvePin& operator=(CurlResolvePin&& other) noexcept;

    [[nodiscard]] static CurlResolvePin create(
        const OriginUrl& origin,
        std::span<const ValidatedAddress> addresses);

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] ResolvePinError error() const noexcept;
    [[nodiscard]] const std::string& entry() const noexcept;
    [[nodiscard]] curl_slist* native_handle() const noexcept;
    [[nodiscard]] bool matches(const OriginUrl& origin) const noexcept;

private:
    explicit CurlResolvePin(ResolvePinError error) noexcept;

    ResolvePinError error_ = ResolvePinError::empty_addresses;
    std::string canonical_url_;
    std::string hostname_;
    std::uint16_t port_ = 0;
    std::string entry_;
    curl_slist* list_ = nullptr;
};

} // namespace mediaproxy::http
