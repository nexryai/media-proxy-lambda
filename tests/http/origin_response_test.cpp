#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/http/origin_response.hpp>

namespace {

using mediaproxy::http::OriginResponseAccumulator;
using mediaproxy::http::OriginResponseError;
using mediaproxy::http::maximum_origin_body_bytes;

struct ContentLengthCase {
    const char* name;
    const char* value;
    OriginResponseError error;
    std::int64_t expected;
};

class ContentLengthTest : public testing::TestWithParam<ContentLengthCase> {
};

TEST_P(ContentLengthTest, ParsesSignedBaseTenWithinRetainedLimit)
{
    const auto& parameter = GetParam();
    OriginResponseAccumulator response;
    const std::string header =
        std::string{"Content-Length: "} + parameter.value + "\r\n";
    response.consume_header_line(header);
    EXPECT_EQ(response.error(), parameter.error);
    if (parameter.error == OriginResponseError::none) {
        ASSERT_TRUE(response.content_length().has_value());
        EXPECT_EQ(*response.content_length(), parameter.expected);
    } else {
        EXPECT_FALSE(response.content_length().has_value());
    }
}

INSTANTIATE_TEST_SUITE_P(
    Specification,
    ContentLengthTest,
    testing::Values(
        ContentLengthCase{"zero", "0", OriginResponseError::none, 0},
        ContentLengthCase{"explicit_plus", "+1", OriginResponseError::none, 1},
        ContentLengthCase{
            "exact_limit",
            "10485760",
            OriginResponseError::none,
            10485760},
        ContentLengthCase{
            "above_limit",
            "10485761",
            OriginResponseError::content_length_too_large,
            0},
        ContentLengthCase{
            "invalid",
            "not-a-number",
            OriginResponseError::invalid_content_length,
            0},
        ContentLengthCase{
            "overflow",
            "9223372036854775808",
            OriginResponseError::invalid_content_length,
            0}),
    [](const testing::TestParamInfo<ContentLengthCase>& info) {
        return info.param.name;
    });

TEST(OriginResponse, TreatsContentLengthNameCaseInsensitivelyAndTrimsOws)
{
    OriginResponseAccumulator response;
    response.consume_header_line("content-length:\t 42 \t\r\n");
    ASSERT_TRUE(response.content_length().has_value());
    EXPECT_EQ(*response.content_length(), 42);
}

TEST(OriginResponse, AppliesExactNextDnsHeaderRule)
{
    OriginResponseAccumulator blocked;
    blocked.consume_header_line("Blocked-By: NextDNS\r\n");
    EXPECT_EQ(blocked.error(), OriginResponseError::blocked_by_nextdns);

    for (const std::string_view variant : std::array{
             std::string_view{"blocked-by: NextDNS\r\n"},
             std::string_view{"Blocked-By: nextdns\r\n"},
             std::string_view{"Blocked-By:  NextDNS\r\n"},
         }) {
        OriginResponseAccumulator allowed;
        allowed.consume_header_line(variant);
        EXPECT_EQ(allowed.error(), OriginResponseError::none) << variant;
    }
}

TEST(OriginResponse, RetainsTrimmedCaseInsensitiveRedirectLocation)
{
    OriginResponseAccumulator response;
    response.consume_header_line("lOcAtIoN:\t ../next?value=1 \t\r\n");
    ASSERT_TRUE(response.location().has_value());
    EXPECT_EQ(*response.location(), "../next?value=1");

    response.consume_header_line("Location: https://next.example/final\r\n");
    ASSERT_TRUE(response.location().has_value());
    EXPECT_EQ(*response.location(), "https://next.example/final");
}

TEST(OriginResponse, RetainsAtMostTenMibWithoutProbeByte)
{
    OriginResponseAccumulator response;
    const std::vector<std::byte> chunk(64U * 1024U, std::byte{0x5a});
    while (response.body().size() < maximum_origin_body_bytes) {
        const std::size_t remaining =
            maximum_origin_body_bytes - response.body().size();
        const std::size_t offered = std::min(remaining, chunk.size());
        EXPECT_EQ(response.append_body(std::span{chunk}.first(offered)), offered);
    }
    ASSERT_TRUE(response.at_body_limit());
    ASSERT_EQ(response.body().size(), maximum_origin_body_bytes);

    constexpr std::array<std::byte, 1> probe{std::byte{0x7f}};
    EXPECT_EQ(response.append_body(probe), 0U);
    EXPECT_EQ(response.body().size(), maximum_origin_body_bytes);
    EXPECT_EQ(response.body().back(), std::byte{0x5a});
    EXPECT_TRUE(response.finish(200));
}

TEST(OriginResponse, TruncatesCrossingCallbackAtExactLimit)
{
    OriginResponseAccumulator response;
    const std::vector<std::byte> prefix(
        maximum_origin_body_bytes - 3U,
        std::byte{0x11});
    ASSERT_EQ(response.append_body(prefix), prefix.size());
    constexpr std::array<std::byte, 8> crossing{
        std::byte{1}, std::byte{2}, std::byte{3}, std::byte{4},
        std::byte{5}, std::byte{6}, std::byte{7}, std::byte{8},
    };
    EXPECT_EQ(response.append_body(crossing), 3U);
    ASSERT_EQ(response.body().size(), maximum_origin_body_bytes);
    EXPECT_EQ(response.body()[maximum_origin_body_bytes - 1], std::byte{3});
}

TEST(OriginResponse, EvaluatesStatusOnlyAfterBodyHasBeenRetained)
{
    OriginResponseAccumulator response;
    constexpr std::array<std::byte, 3> body{
        std::byte{0x62}, std::byte{0x61}, std::byte{0x64}};
    ASSERT_EQ(response.append_body(body), body.size());
    EXPECT_FALSE(response.finish(404));
    EXPECT_EQ(response.error(), OriginResponseError::non_200_status);
    EXPECT_EQ(response.body().size(), body.size());
}

} // namespace
