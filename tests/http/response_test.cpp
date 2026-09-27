#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/http/query.hpp>
#include <mediaproxy/http/response.hpp>

namespace {

using mediaproxy::http::ErrorResponse;
using mediaproxy::http::HttpResponse;
using mediaproxy::http::make_error_response;
using mediaproxy::http::make_media_response;
using mediaproxy::http::make_status_response;
using mediaproxy::http::PreferredOutput;

auto bytes(std::string_view text) -> std::vector<std::byte> {
    const auto bytes = std::as_bytes(std::span{text});

    return {bytes.begin(), bytes.end()};
}

auto header_value(const HttpResponse &response, std::string_view name) -> std::string_view {
    for (const auto &header : response.headers) {
        if (header.name == name) {
            return header.value;
        }
    }

    return {};
}

struct ErrorCase {
    const char *name;
    ErrorResponse error;
    std::uint16_t status;
    const char *body;
};

class ErrorResponseTest : public testing::TestWithParam<ErrorCase> {};

TEST_P(ErrorResponseTest, MatchesExactStatusHeadersAndBody) {
    const ErrorCase &expected = GetParam();
    const HttpResponse response = make_error_response(expected.error);
    EXPECT_EQ(response.status, expected.status);
    ASSERT_EQ(response.headers.size(), 1U);
    EXPECT_EQ(header_value(response, "Content-Type"), "text/plain; charset=utf-8");
    EXPECT_EQ(response.body, bytes(expected.body));
}

INSTANTIATE_TEST_SUITE_P(Specification, ErrorResponseTest,
                         testing::Values(ErrorCase{"bad_request", ErrorResponse::bad_request, 400, "Bad request\n"}, ErrorCase{"access_denied", ErrorResponse::access_denied, 403, "Access denied\n"},
                                         ErrorCase{"invalid_image", ErrorResponse::invalid_image, 400, "Failed to resize image: invalid image?\n"}, ErrorCase{"internal", ErrorResponse::internal, 500, "Internal Server Error\n"}),
                         [](const testing::TestParamInfo<ErrorCase> &info) -> const char * { return info.param.name; });

TEST(Response, BuildsExactStatusResponse) {
    const HttpResponse response = make_status_response();
    EXPECT_EQ(response.status, 200);
    ASSERT_EQ(response.headers.size(), 1U);
    EXPECT_EQ(header_value(response, "Content-Type"), "application/json");
    EXPECT_EQ(response.body, bytes(R"({"status":"OK"})"));
}

TEST(Response, BuildsMediaMetadataWithoutInspectingBinaryBody) {
    const std::vector<std::byte> binary_body{std::byte{0}, std::byte{0xff}};
    const HttpResponse avif = make_media_response(PreferredOutput::avif, binary_body);
    EXPECT_EQ(avif.status, 200);
    EXPECT_EQ(header_value(avif, "Content-Type"), "image/avif");
    EXPECT_EQ(header_value(avif, "CDN-Cache-Control"), "max-age=604800");
    EXPECT_EQ(header_value(avif, "Cache-Control"), "max-age=432000");
    EXPECT_EQ(avif.body, binary_body);

    const HttpResponse webp = make_media_response(PreferredOutput::webp, binary_body);
    EXPECT_EQ(header_value(webp, "Content-Type"), "image/webp");
    EXPECT_EQ(webp.body, binary_body);
}

} // namespace
