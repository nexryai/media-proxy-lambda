#include <mediaproxy/http/curl_resolve_pin.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>

#include <arpa/inet.h>
#include <curl/curl.h>
#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace mediaproxy::http {
namespace {

[[nodiscard]] std::string format_address(const ValidatedAddress& address)
{
    std::array<char, INET6_ADDRSTRLEN> buffer{};
    const int family = address.family == AddressFamily::ipv4
        ? AF_INET
        : AF_INET6;
    if (inet_ntop(
            family,
            address.bytes.data(),
            buffer.data(),
            static_cast<socklen_t>(buffer.size()))
        == nullptr) {
        return {};
    }
    return buffer.data();
}

} // namespace

CurlResolvePin::CurlResolvePin(ResolvePinError error) noexcept
    : error_(error)
{
}

CurlResolvePin::~CurlResolvePin()
{
    curl_slist_free_all(list_);
}

CurlResolvePin::CurlResolvePin(CurlResolvePin&& other) noexcept
    : error_(other.error_)
    , entry_(std::move(other.entry_))
    , list_(std::exchange(other.list_, nullptr))
{
    other.error_ = ResolvePinError::empty_addresses;
}

CurlResolvePin& CurlResolvePin::operator=(CurlResolvePin&& other) noexcept
{
    if (this != &other) {
        curl_slist_free_all(list_);
        error_ = other.error_;
        entry_ = std::move(other.entry_);
        list_ = std::exchange(other.list_, nullptr);
        other.error_ = ResolvePinError::empty_addresses;
    }
    return *this;
}

CurlResolvePin CurlResolvePin::create(
    const OriginUrl& origin,
    std::span<const ValidatedAddress> addresses)
{
    if (origin.hostname.empty() || origin.hostname.find(':') != std::string::npos
        || (origin.port != 80 && origin.port != 443)) {
        return CurlResolvePin{ResolvePinError::invalid_origin};
    }
    if (addresses.empty()) {
        return CurlResolvePin{ResolvePinError::empty_addresses};
    }

    CurlResolvePin pin{ResolvePinError::none};
    pin.entry_ = origin.hostname + ":" + std::to_string(origin.port) + ":";
    for (std::size_t index = 0; index < addresses.size(); ++index) {
        std::string formatted = format_address(addresses[index]);
        if (formatted.empty()) {
            return CurlResolvePin{ResolvePinError::address_format};
        }
        if (index != 0) {
            pin.entry_.push_back(',');
        }
        if (addresses[index].family == AddressFamily::ipv6) {
            pin.entry_.push_back('[');
            pin.entry_.append(formatted);
            pin.entry_.push_back(']');
        } else {
            pin.entry_.append(formatted);
        }
    }

    pin.list_ = curl_slist_append(nullptr, pin.entry_.c_str());
    if (pin.list_ == nullptr) {
        pin.error_ = ResolvePinError::allocation;
        pin.entry_.clear();
    }
    return pin;
}

CurlResolvePin::operator bool() const noexcept
{
    return error_ == ResolvePinError::none && list_ != nullptr;
}

ResolvePinError CurlResolvePin::error() const noexcept
{
    return error_;
}

const std::string& CurlResolvePin::entry() const noexcept
{
    return entry_;
}

curl_slist* CurlResolvePin::native_handle() const noexcept
{
    return list_;
}

} // namespace mediaproxy::http
