#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <curl/curl.h>
#include <gtest/gtest.h>
#include <mediaproxy/handler.hpp>
#include <mediaproxy/http/origin_download.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/response.hpp>
#include <mediaproxy/logging.hpp>
#include <vips/vips.h>

namespace {

using mediaproxy::handle_function_url_event;
using mediaproxy::HandlerDiagnostics;
using mediaproxy::HandlerOutcome;
using mediaproxy::http::HttpResponse;
using mediaproxy::http::OriginResponseAccumulator;
using mediaproxy::http::OriginTimeoutApi;
using mediaproxy::http::OriginTransportApi;

auto to_bytes(std::string_view text) -> std::vector<std::byte> {
    const auto bytes = std::as_bytes(std::span{text});

    return {bytes.begin(), bytes.end()};
}

auto read_file(const std::string &path) -> std::vector<std::byte> {
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input.is_open()) << path;
    const std::string bytes{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};

    return to_bytes(bytes);
}

auto read_stream(std::FILE *stream) -> std::string {
    EXPECT_EQ(std::fflush(stream), 0);
    EXPECT_EQ(std::fseek(stream, 0, SEEK_SET), 0);
    std::string output;
    std::array<char, 512> buffer{};
    while (const std::size_t count = std::fread(buffer.data(), 1, buffer.size(), stream)) {
        output.append(buffer.data(), count);
    }

    return output;
}

auto media_event(std::string_view raw_query) -> std::string {

    return std::string{R"({"version":"2.0","rawPath":"/proxy","rawQueryString":")"} + std::string{raw_query} + R"(","requestContext":{"http":{"method":"GET"}}})";
}

auto header_value(const HttpResponse &response, std::string_view name) -> std::string_view {
    for (const auto &header : response.headers) {
        if (header.name == name) {
            return header.value;
        }
    }

    return {};
}

struct FakeOrigin {
    std::vector<std::byte> body;
    CURLcode perform_result = CURLE_OK;
    long status = 200;
    std::size_t create_calls = 0;
};

auto create_easy(void *context) -> CURL * {
    auto &origin = *static_cast<FakeOrigin *>(context);
    ++origin.create_calls;

    return curl_easy_init();
}

void destroy_easy(CURL *easy, void * /*unused*/) {
    curl_easy_cleanup(easy);
}

auto perform(CURL * /*unused*/, OriginResponseAccumulator &response, void *context) -> CURLcode {
    auto &origin = *static_cast<FakeOrigin *>(context);
    if (origin.perform_result != CURLE_OK) {
        return origin.perform_result;
    }

    return response.append_body(origin.body) == origin.body.size() ? CURLE_OK : CURLE_WRITE_ERROR;
}

auto response_code(CURL * /*unused*/, long *status, void *context) -> CURLcode {

    *status = static_cast<FakeOrigin *>(context)->status;
    return CURLE_OK;
}

auto transport(FakeOrigin &origin) -> OriginTransportApi {

    return {
        .context = &origin,
        .create = &create_easy,
        .destroy = &destroy_easy,
        .perform = &perform,
        .response_code = &response_code,
    };
}

auto remaining_time(void * /*unused*/) -> long {
    return 10'000;
}

auto timeout() -> OriginTimeoutApi {

    return {.context = nullptr, .remaining_milliseconds = &remaining_time};
}

class HandlerTest : public testing::Test {
  protected:
    static void SetUpTestSuite() {
        ASSERT_EQ(curl_global_init(CURL_GLOBAL_DEFAULT), CURLE_OK);
    }

    static void TearDownTestSuite() {
        curl_global_cleanup();
    }
};

TEST_F(HandlerTest, ReturnsStatusWithoutOriginAccess) {
    const std::string event = R"({"version":"2.0","rawPath":"/status","rawQueryString":"","requestContext":{"http":{"method":"GET"}}})";
    FakeOrigin origin;
    const HttpResponse response = handle_function_url_event(std::as_bytes(std::span{event}), {}, {}, transport(origin));
    EXPECT_EQ(response.status, 200);
    EXPECT_EQ(response.body, to_bytes(R"({"status":"OK"})"));
    EXPECT_EQ(origin.create_calls, 0U);
}

TEST_F(HandlerTest, MapsRequestAndInitialUrlFailures) {
    FakeOrigin origin;
    const std::string invalid_event = "{}";
    const HttpResponse bad_request = handle_function_url_event(std::as_bytes(std::span{invalid_event}), {}, {}, transport(origin));
    EXPECT_EQ(bad_request.status, 400);
    EXPECT_EQ(bad_request.body, to_bytes("Bad request\n"));

    const std::string event = media_event("url=http%3A%2F%2Fexample.com%2Fx");
    const HttpResponse denied = handle_function_url_event(std::as_bytes(std::span{event}), timeout(), {}, transport(origin));
    EXPECT_EQ(denied.status, 403);
    EXPECT_EQ(denied.body, to_bytes("Access denied\n"));
    EXPECT_EQ(origin.create_calls, 0U);
}

TEST_F(HandlerTest, MapsDownloadAndImageFailuresSeparately) {
    const std::string event = media_event("url=https%3A%2F%2F93.184.216.34%2Fimage");
    FakeOrigin origin;
    origin.perform_result = CURLE_OPERATION_TIMEDOUT;
    const HttpResponse download_failure = handle_function_url_event(std::as_bytes(std::span{event}), timeout(), {}, transport(origin));
    EXPECT_EQ(download_failure.status, 500);
    EXPECT_EQ(download_failure.body, to_bytes("Internal Server Error\n"));

    origin.perform_result = CURLE_OK;
    origin.body = to_bytes("not an image");
    const HttpResponse image_failure = handle_function_url_event(std::as_bytes(std::span{event}), timeout(), {}, transport(origin));
    EXPECT_EQ(image_failure.status, 400);
    EXPECT_EQ(image_failure.body, to_bytes("Failed to resize image: invalid image?\n"));
}

TEST_F(HandlerTest, ConvertsDownloadedMediaIntoPreferredResponse) {
    FakeOrigin origin;
    origin.body = read_file(std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/fixtures/media/apng/palette-alpha.png");
    const std::string event = media_event("url=https%3A%2F%2F93.184.216.34%2Fimage");
    HandlerDiagnostics diagnostics;
    const HttpResponse response = handle_function_url_event(std::as_bytes(std::span{event}), timeout(), {}, transport(origin), &diagnostics);
    ASSERT_EQ(response.status, 200);
    EXPECT_EQ(header_value(response, "Content-Type"), "image/webp");
    EXPECT_EQ(header_value(response, "CDN-Cache-Control"), "max-age=604800");
    ASSERT_GE(response.body.size(), 12U);
    EXPECT_EQ(std::string_view(reinterpret_cast<const char *>(response.body.data()), 4), "RIFF");
    EXPECT_EQ(std::string_view(reinterpret_cast<const char *>(response.body.data() + 8), 4), "WEBP");
    EXPECT_EQ(diagnostics.outcome, HandlerOutcome::media_success);
    EXPECT_EQ(diagnostics.origin_bytes, origin.body.size());
    EXPECT_EQ(diagnostics.origin_error, mediaproxy::http::OriginDownloadError::none);
    EXPECT_EQ(diagnostics.media_error, mediaproxy::media::MediaConversionError::none);
    EXPECT_EQ(vips_cache_get_size(), 0);
}

TEST_F(HandlerTest, UrlOnlyRequestUsesHigherWebpQuality) {
    FakeOrigin origin;
    origin.body = read_file(std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/fixtures/media/resize/1500x843.jpg");
    ASSERT_FALSE(origin.body.empty());
    const std::string url_query = "url=https%3A%2F%2F93.184.216.34%2Fimage";
    const auto url_only = media_event(url_query);
    const auto with_extra = media_event(url_query + "&unused=1");
    const auto high_quality = handle_function_url_event(std::as_bytes(std::span{url_only}), timeout(), {}, transport(origin));
    const auto standard_quality = handle_function_url_event(std::as_bytes(std::span{with_extra}), timeout(), {}, transport(origin));
    ASSERT_EQ(high_quality.status, 200);
    ASSERT_EQ(standard_quality.status, 200);
    EXPECT_EQ(header_value(high_quality, "Content-Type"), "image/webp");
    EXPECT_EQ(header_value(standard_quality, "Content-Type"), "image/webp");
    EXPECT_NE(high_quality.body, standard_quality.body);
}

TEST_F(HandlerTest, LogsBoundedDiagnosticsWithoutRequestData) {
    std::FILE *const stream = std::tmpfile();
    ASSERT_NE(stream, nullptr);
    HandlerDiagnostics diagnostics{
        .outcome = HandlerOutcome::origin_failure,
        .origin_error = mediaproxy::http::OriginDownloadError::transfer,
        .media_error = mediaproxy::media::MediaConversionError::none,
        .origin_bytes = 123,
        .fetch_microseconds = 456,
        .media_microseconds = 0,
    };
    mediaproxy::log_invocation(stream, "request-\"1", diagnostics, 500, 1000, 22, 789);
    const std::string output = read_stream(stream);
    std::fclose(stream);

    EXPECT_EQ(output, "{\"category\":\"invocation\",\"requestId\":\"request-\\\"1\""
                      ",\"outcome\":\"origin_failure\",\"originError\":\"transfer\""
                      ",\"mediaError\":\"none\",\"status\":500,\"eventBytes\":1000"
                      ",\"originBytes\":123,\"responseBytes\":22,\"fetchMicros\":456"
                      ",\"mediaMicros\":0,\"handlerMicros\":789}\n");
    EXPECT_EQ(output.find("url"), std::string::npos);
    EXPECT_EQ(output.find("query"), std::string::npos);
}

} // namespace
