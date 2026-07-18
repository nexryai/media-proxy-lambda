#include <cstddef>
#include <optional>
#include <string>
#include <utility>

#include <gtest/gtest.h>
#include <mediaproxy/http/redirect_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace {

using mediaproxy::http::RedirectError;
using mediaproxy::http::RedirectTracker;
using mediaproxy::http::UrlError;
using mediaproxy::http::maximum_origin_redirects;
using mediaproxy::http::validate_origin_url;

std::optional<RedirectTracker> MakeTracker(const char* initial)
{
    const auto parsed = validate_origin_url(initial);
    if (!parsed) {
        return std::nullopt;
    }
    return RedirectTracker::create(*parsed.url);
}

TEST(RedirectPolicy, ResolvesRelativeQueryAndNetworkLocations)
{
    auto created = MakeTracker(
        "https://origin.example/a/b/source.png?old=1");
    ASSERT_TRUE(created.has_value());
    auto tracker = std::move(*created);

    const auto relative = tracker.follow("../image.png?x=1#not-sent");
    ASSERT_TRUE(relative);
    EXPECT_EQ(
        relative.url->canonical_url,
        "https://origin.example/a/image.png?x=1");
    EXPECT_EQ(relative.url->request_target, "/a/image.png?x=1");

    const auto query = tracker.follow("?next=1");
    ASSERT_TRUE(query);
    EXPECT_EQ(
        query.url->canonical_url,
        "https://origin.example/a/image.png?next=1");

    const auto network = tracker.follow("//bücher.example/new");
    ASSERT_TRUE(network);
    EXPECT_EQ(network.url->hostname, "xn--bcher-kva.example");
    EXPECT_EQ(
        network.url->canonical_url,
        "https://xn--bcher-kva.example/new");
    EXPECT_EQ(tracker.redirect_count(), 3U);
}

TEST(RedirectPolicy, ReappliesSyntaxAndLiteralAddressPolicy)
{
    auto created = MakeTracker("https://origin.example/start");
    ASSERT_TRUE(created.has_value());
    auto tracker = std::move(*created);

    const auto insecure = tracker.follow("http://public.example/image");
    EXPECT_FALSE(insecure);
    EXPECT_EQ(insecure.error, RedirectError::url_policy);
    EXPECT_EQ(insecure.url_error, UrlError::invalid_scheme);

    const auto uppercase = tracker.follow("HTTPS://public.example/image");
    EXPECT_FALSE(uppercase);
    EXPECT_EQ(uppercase.error, RedirectError::url_policy);
    EXPECT_EQ(uppercase.url_error, UrlError::invalid_scheme);

    const auto credentials =
        tracker.follow("//user:secret@public.example/image");
    EXPECT_FALSE(credentials);
    EXPECT_EQ(credentials.error, RedirectError::url_policy);
    EXPECT_EQ(credentials.url_error, UrlError::user_information);

    const auto private_literal = tracker.follow("https://127.0.0.1/image");
    EXPECT_FALSE(private_literal);
    EXPECT_EQ(private_literal.error, RedirectError::url_policy);
    EXPECT_EQ(private_literal.url_error, UrlError::forbidden_address);

    const auto public_literal = tracker.follow("https://1.1.1.1/image");
    ASSERT_TRUE(public_literal);
    EXPECT_TRUE(public_literal.url->literal_address.has_value());
    EXPECT_EQ(tracker.redirect_count(), 1U);
}

TEST(RedirectPolicy, DetectsCanonicalLoopsWithoutMutatingState)
{
    auto created = MakeTracker("https://origin.example/path?x=1");
    ASSERT_TRUE(created.has_value());
    auto tracker = std::move(*created);

    const auto fragment = tracker.follow("#fragment");
    EXPECT_FALSE(fragment);
    EXPECT_EQ(fragment.error, RedirectError::loop);
    EXPECT_EQ(tracker.redirect_count(), 0U);
    EXPECT_EQ(
        tracker.current().canonical_url,
        "https://origin.example/path?x=1");

    const auto empty = tracker.follow("");
    EXPECT_FALSE(empty);
    EXPECT_EQ(empty.error, RedirectError::loop);

    const auto explicit_default_port =
        tracker.follow("https://origin.example:443/path?x=1");
    EXPECT_FALSE(explicit_default_port);
    EXPECT_EQ(explicit_default_port.error, RedirectError::loop);

    const auto next = tracker.follow("/next");
    ASSERT_TRUE(next);
    const auto back = tracker.follow("/path?x=1");
    EXPECT_FALSE(back);
    EXPECT_EQ(back.error, RedirectError::loop);
    EXPECT_EQ(tracker.current().canonical_url, "https://origin.example/next");
    EXPECT_EQ(tracker.redirect_count(), 1U);
}

TEST(RedirectPolicy, EnforcesTenSuccessfulRedirects)
{
    auto created = MakeTracker("https://origin.example/start");
    ASSERT_TRUE(created.has_value());
    auto tracker = std::move(*created);
    for (std::size_t index = 0; index < maximum_origin_redirects; ++index) {
        const auto result = tracker.follow("/hop-" + std::to_string(index));
        ASSERT_TRUE(result) << index;
    }
    EXPECT_EQ(tracker.redirect_count(), maximum_origin_redirects);

    const std::string last_url = tracker.current().canonical_url;
    const auto excess = tracker.follow("/one-too-many");
    EXPECT_FALSE(excess);
    EXPECT_EQ(excess.error, RedirectError::too_many_redirects);
    EXPECT_EQ(tracker.current().canonical_url, last_url);
    EXPECT_EQ(tracker.redirect_count(), maximum_origin_redirects);
}

TEST(RedirectPolicy, RejectsMalformedLocationsAndForgedInitialState)
{
    auto created = MakeTracker("https://origin.example/start");
    ASSERT_TRUE(created.has_value());
    auto tracker = std::move(*created);
    constexpr char embedded_nul_source[] = "/safe\0https://attacker.example/";
    const std::string embedded_nul{
        embedded_nul_source, sizeof(embedded_nul_source) - 1};
    const auto malformed = tracker.follow(embedded_nul);
    EXPECT_FALSE(malformed);
    EXPECT_EQ(malformed.error, RedirectError::invalid_location);
    EXPECT_EQ(tracker.redirect_count(), 0U);

    const auto parsed = validate_origin_url("https://origin.example/start");
    ASSERT_TRUE(parsed);
    auto forged = *parsed.url;
    forged.hostname = "attacker.example";
    EXPECT_FALSE(RedirectTracker::create(forged).has_value());

    auto forged_literal = *parsed.url;
    const auto literal = validate_origin_url("https://1.1.1.1/");
    ASSERT_TRUE(literal);
    forged_literal.literal_address = literal.url->literal_address;
    EXPECT_FALSE(RedirectTracker::create(forged_literal).has_value());
}

} // namespace
