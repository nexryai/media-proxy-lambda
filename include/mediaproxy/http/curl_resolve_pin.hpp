#pragma once

#include <cstdint>
#include <span>
#include <string>

#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

// NOLINTNEXTLINE(readability-identifier-naming): preserve libcurl's C API type name.
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

    CurlResolvePin(const CurlResolvePin &) = delete;
    auto operator=(const CurlResolvePin &) -> CurlResolvePin & = delete;
    CurlResolvePin(CurlResolvePin &&other) noexcept;
    auto operator=(CurlResolvePin &&other) noexcept -> CurlResolvePin &;

    [[nodiscard]] static auto create(const OriginUrl &origin, std::span<const ValidatedAddress> addresses) -> CurlResolvePin;

    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] auto error() const noexcept -> ResolvePinError;
    [[nodiscard]] auto entry() const noexcept -> const std::string &;
    [[nodiscard]] auto native_handle() const noexcept -> curl_slist *;
    [[nodiscard]] auto matches(const OriginUrl &origin) const noexcept -> bool;

  private:
    explicit CurlResolvePin(ResolvePinError error) noexcept;

    ResolvePinError error_ = ResolvePinError::empty_addresses;
    std::string canonical_url_;
    std::string hostname_;
    std::uint16_t port_ = 0;
    std::string entry_;
    curl_slist *list_ = nullptr;
};

} // namespace mediaproxy::http
