#include <mediaproxy/http/event.hpp>

#include "percent_decode.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <yyjson.h>

namespace mediaproxy::http {
namespace {

[[nodiscard]] EventParseResult fail(EventError error)
{
    return {.request = std::nullopt, .error = error};
}

[[nodiscard]] std::optional<std::string_view> json_string(
    yyjson_val* object,
    const char* name) noexcept
{
    yyjson_val* const value = yyjson_obj_get(object, name);
    if (!yyjson_is_str(value)) {
        return std::nullopt;
    }
    return std::string_view{yyjson_get_str(value), yyjson_get_len(value)};
}

} // namespace

EventParseResult parse_function_url_event(std::string_view payload)
{
    std::string json{payload};
    yyjson_read_err read_error{};
    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> document(
        yyjson_read_opts(
            json.data(),
            json.size(),
            YYJSON_READ_NOFLAG,
            nullptr,
            &read_error),
        &yyjson_doc_free);
    if (!document) {
        return fail(EventError::invalid_json);
    }

    yyjson_val* const root = yyjson_doc_get_root(document.get());
    if (!yyjson_is_obj(root)) {
        return fail(EventError::invalid_structure);
    }
    const auto version = json_string(root, "version");
    const auto raw_path = json_string(root, "rawPath");
    const auto raw_query = json_string(root, "rawQueryString");
    yyjson_val* const request_context = yyjson_obj_get(root, "requestContext");
    yyjson_val* const http = yyjson_is_obj(request_context)
        ? yyjson_obj_get(request_context, "http")
        : nullptr;
    const auto method = yyjson_is_obj(http)
        ? json_string(http, "method")
        : std::nullopt;
    if (!version || *version != "2.0" || !raw_path || !raw_query || !method) {
        return fail(EventError::invalid_structure);
    }

    auto decoded_path = detail::percent_decode(*raw_path, false);
    if (!decoded_path) {
        return fail(EventError::invalid_path_escape);
    }
    const RequestRoute route = *decoded_path == "/status"
        ? RequestRoute::status
        : RequestRoute::media;
    FunctionUrlRequest request{
        .method = std::string{*method},
        .decoded_path = std::move(*decoded_path),
        .query = parse_query(*raw_query),
        .route = route,
    };
    return {.request = std::move(request), .error = EventError::none};
}

} // namespace mediaproxy::http
