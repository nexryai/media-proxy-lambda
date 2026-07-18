#pragma once

#include <optional>
#include <string>
#include <string_view>

#include <mediaproxy/http/query.hpp>

namespace mediaproxy::http {

enum class RequestRoute {
    status,
    media,
};

struct FunctionUrlRequest {
    std::string method;
    std::string decoded_path;
    QueryParameters query;
    RequestRoute route = RequestRoute::media;
};

enum class EventError {
    none,
    invalid_json,
    invalid_structure,
    invalid_path_escape,
};

struct EventParseResult {
    std::optional<FunctionUrlRequest> request;
    EventError error = EventError::none;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return request.has_value();
    }
};

[[nodiscard]] EventParseResult parse_function_url_event(
    std::string_view payload);

} // namespace mediaproxy::http
