#include <array>
#include <cstddef>
#include <limits>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <curl/curl.h>
#include <gtest/gtest.h>
#include <mediaproxy/http/ca_bundle.hpp>
#include <mediaproxy/http/curl_resolve_pin.hpp>
#include <mediaproxy/http/dns_policy.hpp>
#include <mediaproxy/http/origin_curl.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace {

using mediaproxy::http::CurlResolvePin;
using mediaproxy::http::OriginCurlConfigError;
using mediaproxy::http::OriginResponseAccumulator;
using mediaproxy::http::OriginResponseError;
using mediaproxy::http::configure_origin_curl;
using mediaproxy::http::embedded_ca_bundle;
using mediaproxy::http::is_body_limit_completion;
using mediaproxy::http::maximum_origin_body_bytes;
using mediaproxy::http::origin_body_callback;
using mediaproxy::http::origin_header_callback;
using mediaproxy::http::validate_origin_url;
using mediaproxy::http::validate_resolved_addresses;

class CurlGlobal final {
public:
    CurlGlobal() noexcept
        : result_(curl_global_init(CURL_GLOBAL_DEFAULT))
    {
    }

    ~CurlGlobal()
    {
        if (result_ == CURLE_OK) {
            curl_global_cleanup();
        }
    }

    [[nodiscard]] CURLcode result() const noexcept
    {
        return result_;
    }

private:
    CURLcode result_;
};

TEST(OriginCurl, ConfiguresPinnedVerifiedGetRequest)
{
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> easy(
        curl_easy_init(),
        &curl_easy_cleanup);
    ASSERT_NE(easy, nullptr);

    const auto parsed = validate_origin_url("https://origin.example/image.png");
    ASSERT_TRUE(parsed);
    constexpr std::array<std::string_view, 1> candidates{"1.1.1.1"};
    const auto resolved = validate_resolved_addresses(candidates);
    ASSERT_TRUE(resolved);
    const auto pin = CurlResolvePin::create(*parsed.url, resolved.addresses);
    ASSERT_TRUE(pin);
    OriginResponseAccumulator response;
    EXPECT_EQ(
        configure_origin_curl(
            easy.get(), *parsed.url, pin, response, 1500),
        OriginCurlConfigError::none);
}

TEST(OriginCurl, EmbedsPinnedMozillaTrustBundle)
{
    const std::span<const std::byte> bundle = embedded_ca_bundle();
    ASSERT_EQ(bundle.size(), 186446U);
    const std::string_view text{
        reinterpret_cast<const char*>(bundle.data()), bundle.size()};
    EXPECT_TRUE(text.starts_with("##\n## Bundle of CA Root Certificates\n"));
    EXPECT_NE(
        text.find("Certificate data from Mozilla as of: "
                  "Thu Jul 16 03:12:01 2026 GMT"),
        std::string_view::npos);

    constexpr std::string_view certificate_begin =
        "-----BEGIN CERTIFICATE-----";
    std::size_t certificate_count = 0;
    for (std::size_t offset = 0;
         (offset = text.find(certificate_begin, offset))
            != std::string_view::npos;
         offset += certificate_begin.size()) {
        ++certificate_count;
    }
    EXPECT_EQ(certificate_count, 119U);
}

TEST(OriginCurl, RejectsInvalidArgumentsAndMismatchedPin)
{
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> easy(
        curl_easy_init(),
        &curl_easy_cleanup);
    ASSERT_NE(easy, nullptr);
    const auto first = validate_origin_url("https://first.example/");
    const auto second = validate_origin_url("https://second.example/");
    ASSERT_TRUE(first);
    ASSERT_TRUE(second);
    constexpr std::array<std::string_view, 1> candidates{"1.1.1.1"};
    const auto resolved = validate_resolved_addresses(candidates);
    ASSERT_TRUE(resolved);
    const auto pin = CurlResolvePin::create(*first.url, resolved.addresses);
    ASSERT_TRUE(pin);
    OriginResponseAccumulator response;
    EXPECT_EQ(
        configure_origin_curl(
            nullptr, *first.url, pin, response, 1000),
        OriginCurlConfigError::invalid_argument);
    EXPECT_EQ(
        configure_origin_curl(
            easy.get(), *second.url, pin, response, 1000),
        OriginCurlConfigError::invalid_argument);
    auto changed_url = *first.url;
    changed_url.canonical_url = "https://second.example/";
    EXPECT_EQ(
        configure_origin_curl(
            easy.get(), changed_url, pin, response, 1000),
        OriginCurlConfigError::invalid_argument);
    EXPECT_EQ(
        configure_origin_curl(
            easy.get(), *first.url, pin, response, 0),
        OriginCurlConfigError::invalid_argument);
}

TEST(OriginCurl, CallbacksContainErrorsAndRetainBoundedBytes)
{
    OriginResponseAccumulator response;
    std::array<char, 20> length_header{
        'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'L', 'e',
        'n', 'g', 't', 'h', ':', ' ', '3', '\r', '\n', '\0'};
    EXPECT_EQ(
        origin_header_callback(
            length_header.data(), 1, length_header.size() - 1, &response),
        length_header.size() - 1);
    ASSERT_TRUE(response.content_length().has_value());
    EXPECT_EQ(*response.content_length(), 3);

    std::array<char, 3> body{'a', 'b', 'c'};
    EXPECT_EQ(origin_body_callback(body.data(), 1, body.size(), &response), 3U);
    EXPECT_EQ(response.body().size(), 3U);

    EXPECT_EQ(origin_body_callback(nullptr, 1, 1, &response), 0U);
    EXPECT_EQ(
        origin_body_callback(
            body.data(),
            std::numeric_limits<std::size_t>::max(),
            2,
            &response),
        0U);

    OriginResponseAccumulator blocked;
    std::array<char, 20> blocked_header{
        'B', 'l', 'o', 'c', 'k', 'e', 'd', '-', 'B', 'y',
        ':', ' ', 'N', 'e', 'x', 't', 'D', 'N', 'S', '\n'};
    EXPECT_EQ(
        origin_header_callback(
            blocked_header.data(), 1, blocked_header.size(), &blocked),
        0U);
    EXPECT_EQ(blocked.error(), OriginResponseError::blocked_by_nextdns);
}

TEST(OriginCurl, RecognizesOnlyIntentionalBodyLimitWriteAbort)
{
    OriginResponseAccumulator response;
    const std::vector<std::byte> body(
        maximum_origin_body_bytes,
        std::byte{0x5a});
    ASSERT_EQ(response.append_body(body), body.size());
    ASSERT_TRUE(response.at_body_limit());
    EXPECT_TRUE(is_body_limit_completion(CURLE_WRITE_ERROR, response));
    EXPECT_FALSE(is_body_limit_completion(CURLE_OK, response));

    OriginResponseAccumulator short_response;
    EXPECT_FALSE(
        is_body_limit_completion(CURLE_WRITE_ERROR, short_response));
}

} // namespace
