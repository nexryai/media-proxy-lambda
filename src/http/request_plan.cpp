#include <mediaproxy/http/request_plan.hpp>

#include <string>
#include <string_view>

namespace mediaproxy::http {

RequestPlan plan_request(const EventParseResult& event)
{
    if (!event.request) {
        return make_error_response(ErrorResponse::bad_request);
    }
    const FunctionUrlRequest& request = *event.request;
    if (request.route == RequestRoute::status) {
        return make_status_response();
    }

    const std::string_view source_url = request.query.first("url");
    if (source_url.empty()) {
        return make_error_response(ErrorResponse::bad_request);
    }
    return MediaRequest{
        .method = request.method,
        .decoded_path = request.decoded_path,
        .source_url = std::string{source_url},
        .options = select_media_options(request.query),
    };
}

} // namespace mediaproxy::http
