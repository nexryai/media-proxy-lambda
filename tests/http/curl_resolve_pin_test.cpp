#include <array>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>
#include <mediaproxy/http/curl_resolve_pin.hpp>
#include <mediaproxy/http/dns_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace {

using mediaproxy::http::CurlResolvePin;
using mediaproxy::http::OriginUrl;
using mediaproxy::http::ResolvePinError;
using mediaproxy::http::validate_origin_url;
using mediaproxy::http::validate_resolved_addresses;

TEST(CurlResolvePin, PinsMixedPublicAddressesInResolverOrder)
{
    const auto parsed = validate_origin_url("https://origin.example/image.png");
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed.url.has_value());
    constexpr std::array<std::string_view, 3> candidates{
        "1.1.1.1",
        "2606:4700:4700::1111",
        "8.8.8.8",
    };
    const auto resolved = validate_resolved_addresses(candidates);
    ASSERT_TRUE(resolved);

    auto pin = CurlResolvePin::create(*parsed.url, resolved.addresses);
    ASSERT_TRUE(pin);
    EXPECT_EQ(pin.error(), ResolvePinError::none);
    EXPECT_EQ(
        pin.entry(),
        "origin.example:443:1.1.1.1,[2606:4700:4700::1111],8.8.8.8");
    EXPECT_NE(pin.native_handle(), nullptr);
}

TEST(CurlResolvePin, RetainsCanonicalHostAndExplicitPort)
{
    const auto parsed =
        validate_origin_url("https://Origin.Example:80/image.png");
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed.url.has_value());
    constexpr std::array<std::string_view, 1> candidates{"1.1.1.1"};
    const auto resolved = validate_resolved_addresses(candidates);
    ASSERT_TRUE(resolved);

    const auto pin = CurlResolvePin::create(*parsed.url, resolved.addresses);
    ASSERT_TRUE(pin);
    EXPECT_EQ(pin.entry(), "origin.example:80:1.1.1.1");
}

TEST(CurlResolvePin, RejectsEmptyCandidatesAndInvalidOrigins)
{
    const auto parsed = validate_origin_url("https://origin.example/");
    ASSERT_TRUE(parsed);
    ASSERT_TRUE(parsed.url.has_value());
    constexpr std::array<mediaproxy::http::ValidatedAddress, 0> empty{};
    const auto empty_pin = CurlResolvePin::create(*parsed.url, empty);
    EXPECT_FALSE(empty_pin);
    EXPECT_EQ(empty_pin.error(), ResolvePinError::empty_addresses);

    OriginUrl invalid_origin = *parsed.url;
    invalid_origin.hostname = "bad:host";
    constexpr std::array<std::string_view, 1> candidates{"1.1.1.1"};
    const auto resolved = validate_resolved_addresses(candidates);
    ASSERT_TRUE(resolved);
    const auto invalid_pin =
        CurlResolvePin::create(invalid_origin, resolved.addresses);
    EXPECT_FALSE(invalid_pin);
    EXPECT_EQ(invalid_pin.error(), ResolvePinError::invalid_origin);
}

TEST(CurlResolvePin, TransfersNativeListOwnershipOnMove)
{
    const auto parsed = validate_origin_url("https://origin.example/");
    ASSERT_TRUE(parsed);
    constexpr std::array<std::string_view, 1> candidates{"1.1.1.1"};
    const auto resolved = validate_resolved_addresses(candidates);
    ASSERT_TRUE(resolved);
    auto source = CurlResolvePin::create(*parsed.url, resolved.addresses);
    ASSERT_TRUE(source);
    auto* const native = source.native_handle();

    CurlResolvePin destination{std::move(source)};
    EXPECT_FALSE(source);
    EXPECT_EQ(source.native_handle(), nullptr);
    ASSERT_TRUE(destination);
    EXPECT_EQ(destination.native_handle(), native);
    EXPECT_EQ(destination.entry(), "origin.example:443:1.1.1.1");
}

} // namespace
