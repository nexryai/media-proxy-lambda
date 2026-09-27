#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/http/event.hpp>
#include <mediaproxy/http/query.hpp>
#include <mediaproxy/http/request_plan.hpp>
#include <mediaproxy/http/response.hpp>
#include <yyjson.h>

namespace {

using mediaproxy::http::EventError;
using mediaproxy::http::HttpResponse;
using mediaproxy::http::MediaRequest;
using mediaproxy::http::MediaSelector;
using mediaproxy::http::parse_function_url_event;
using mediaproxy::http::plan_request;
using mediaproxy::http::PreferredOutput;
using mediaproxy::http::RequestRoute;

auto bytes(std::string_view text) -> std::vector<std::byte> {
    const auto bytes = std::as_bytes(std::span{text});

    return {bytes.begin(), bytes.end()};
}

auto read_file(const std::string &path) -> std::string {
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input.is_open()) << path;

    return {std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
}

auto load_event_manifest() -> std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> {
    const std::string path = std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/fixtures/events/manifest.json";
    std::string json = read_file(path);
    yyjson_read_err error{};
    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> document(yyjson_read_opts(json.data(), json.size(), YYJSON_READ_NOFLAG, nullptr, &error), &yyjson_doc_free);
    EXPECT_NE(document, nullptr) << error.msg;

    return document;
}

auto parse_selector(std::string_view value) -> MediaSelector {
    if (value == "avatar") {
        return MediaSelector::avatar;
    }
    ADD_FAILURE() << "unexpected selector " << value;

    return MediaSelector::default_media;
}

auto parse_output(std::string_view value) -> PreferredOutput {
    if (value == "avif") {
        return PreferredOutput::avif;
    }
    ADD_FAILURE() << "unexpected preferred output " << value;

    return PreferredOutput::webp;
}

auto header_value(const HttpResponse &response, std::string_view name) -> std::string_view {
    for (const auto &header : response.headers) {
        if (header.name == name) {
            return header.value;
        }
    }

    return {};
}

} // namespace

TEST(Event, MatchesCheckedInFunctionUrlFixtures) {
    const auto manifest = load_event_manifest();
    ASSERT_NE(manifest, nullptr);
    yyjson_val *const root = yyjson_doc_get_root(manifest.get());
    ASSERT_TRUE(yyjson_is_obj(root));
    yyjson_val *const fixtures = yyjson_obj_get(root, "fixtures");
    ASSERT_TRUE(yyjson_is_arr(fixtures));

    std::size_t index = 0;
    std::size_t maximum = 0;
    yyjson_val *fixture = nullptr;
    yyjson_arr_foreach(fixtures, index, maximum, fixture) {
        ASSERT_TRUE(yyjson_is_obj(fixture));
        const char *const id = yyjson_get_str(yyjson_obj_get(fixture, "id"));
        const char *const file = yyjson_get_str(yyjson_obj_get(fixture, "file"));
        yyjson_val *const expected = yyjson_obj_get(fixture, "expected");
        ASSERT_NE(id, nullptr);
        ASSERT_NE(file, nullptr);
        ASSERT_TRUE(yyjson_is_obj(expected));
        SCOPED_TRACE(id);

        const std::string path = std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/fixtures/events/" + file;
        const auto result = parse_function_url_event(read_file(path));
        const char *const expected_route = yyjson_get_str(yyjson_obj_get(expected, "route"));
        ASSERT_NE(expected_route, nullptr);
        if (std::string_view{expected_route} == "error") {
            EXPECT_FALSE(result);
            EXPECT_EQ(result.error, EventError::invalid_path_escape);
            const auto plan = plan_request(result);
            ASSERT_TRUE(std::holds_alternative<HttpResponse>(plan));
            const auto &response = std::get<HttpResponse>(plan);
            EXPECT_EQ(response.status, yyjson_get_uint(yyjson_obj_get(expected, "status")));
            EXPECT_EQ(response.body, bytes(yyjson_get_str(yyjson_obj_get(expected, "bodyUtf8"))));
            yyjson_val *const headers = yyjson_obj_get(expected, "headers");
            EXPECT_EQ(header_value(response, "Content-Type"), yyjson_get_str(yyjson_obj_get(headers, "Content-Type")));
            continue;
        }

        ASSERT_TRUE(result);
        ASSERT_TRUE(result.request.has_value());
        EXPECT_EQ(result.error, EventError::none);
        const auto &request = *result.request;
        EXPECT_EQ(request.method, yyjson_get_str(yyjson_obj_get(expected, "method")));
        EXPECT_EQ(request.decoded_path, yyjson_get_str(yyjson_obj_get(expected, "decodedPath")));
        const RequestRoute route = std::string_view{expected_route} == "status" ? RequestRoute::status : RequestRoute::media;
        EXPECT_EQ(request.route, route);

        const auto plan = plan_request(result);
        if (route == RequestRoute::status) {
            ASSERT_TRUE(std::holds_alternative<HttpResponse>(plan));
            const auto &response = std::get<HttpResponse>(plan);
            EXPECT_EQ(response.status, yyjson_get_uint(yyjson_obj_get(expected, "status")));
            EXPECT_EQ(response.body, bytes(yyjson_get_str(yyjson_obj_get(expected, "bodyUtf8"))));
        } else {
            ASSERT_TRUE(std::holds_alternative<MediaRequest>(plan));
            const auto &media_request = std::get<MediaRequest>(plan);
            EXPECT_EQ(media_request.source_url, yyjson_get_str(yyjson_obj_get(expected, "firstUrl")));
            EXPECT_EQ(media_request.method, request.method);
            EXPECT_EQ(media_request.decoded_path, request.decoded_path);
            const auto &options = media_request.options;
            EXPECT_EQ(options.selector, parse_selector(yyjson_get_str(yyjson_obj_get(expected, "selector"))));
            EXPECT_EQ(options.width_limit, yyjson_get_uint(yyjson_obj_get(expected, "widthLimit")));
            EXPECT_EQ(options.height_limit, yyjson_get_uint(yyjson_obj_get(expected, "heightLimit")));
            EXPECT_EQ(options.preferred_output, parse_output(yyjson_get_str(yyjson_obj_get(expected, "preferredOutput"))));
            EXPECT_EQ(options.force_static, yyjson_get_bool(yyjson_obj_get(expected, "static")));
        }
    }
}

TEST(Event, RejectsMalformedJsonAndRequiredFields) {
    const auto invalid_json = parse_function_url_event("{");
    EXPECT_FALSE(invalid_json);
    EXPECT_EQ(invalid_json.error, EventError::invalid_json);

    constexpr std::string_view wrong_version = R"({
        "version":"1.0",
        "rawPath":"/status",
        "rawQueryString":"",
        "requestContext":{"http":{"method":"GET"}}
    })";
    const auto invalid_structure = parse_function_url_event(wrong_version);
    EXPECT_FALSE(invalid_structure);
    EXPECT_EQ(invalid_structure.error, EventError::invalid_structure);

    constexpr std::string_view missing_method = R"({
        "version":"2.0",
        "rawPath":"/status",
        "rawQueryString":"",
        "requestContext":{"http":{}}
    })";
    const auto missing_field = parse_function_url_event(missing_method);
    EXPECT_FALSE(missing_field);
    EXPECT_EQ(missing_field.error, EventError::invalid_structure);
}

TEST(RequestPlan, RejectsMissingOrEmptyFirstUrl) {
    constexpr std::string_view missing_url = R"({
        "version":"2.0",
        "rawPath":"/media",
        "rawQueryString":"thumbnail=1",
        "requestContext":{"http":{"method":"GET"}}
    })";
    const auto missing_plan = plan_request(parse_function_url_event(missing_url));
    ASSERT_TRUE(std::holds_alternative<HttpResponse>(missing_plan));
    EXPECT_EQ(std::get<HttpResponse>(missing_plan).status, 400);

    constexpr std::string_view empty_first_url = R"({
        "version":"2.0",
        "rawPath":"/media",
        "rawQueryString":"url=&url=https%3A%2F%2Forigin.example%2Fa",
        "requestContext":{"http":{"method":"GET"}}
    })";
    const auto empty_plan = plan_request(parse_function_url_event(empty_first_url));
    ASSERT_TRUE(std::holds_alternative<HttpResponse>(empty_plan));
    EXPECT_EQ(std::get<HttpResponse>(empty_plan).status, 400);
}
