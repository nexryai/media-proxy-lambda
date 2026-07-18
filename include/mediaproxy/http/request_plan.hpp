#pragma once

#include <string>
#include <variant>

#include <mediaproxy/http/event.hpp>
#include <mediaproxy/http/query.hpp>
#include <mediaproxy/http/response.hpp>

namespace mediaproxy::http {

struct MediaRequest {
    std::string method;
    std::string decoded_path;
    std::string source_url;
    MediaOptions options;
};

using RequestPlan = std::variant<HttpResponse, MediaRequest>;

[[nodiscard]] RequestPlan plan_request(const EventParseResult& event);

} // namespace mediaproxy::http
