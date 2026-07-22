#include <mediaproxy/handler.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
#include <variant>

#include <mediaproxy/http/event.hpp>
#include <mediaproxy/http/origin_download.hpp>
#include <mediaproxy/http/query.hpp>
#include <mediaproxy/http/request_plan.hpp>
#include <mediaproxy/http/response.hpp>
#include <mediaproxy/http/url_policy.hpp>
#include <mediaproxy/media/classification.hpp>
#include <mediaproxy/media/conversion.hpp>
#include <mediaproxy/media/mime.hpp>
#include <mediaproxy/media/resize.hpp>

namespace mediaproxy {
namespace {

[[nodiscard]] media::OutputFormat preferred_output(
    http::PreferredOutput output) noexcept
{
    return output == http::PreferredOutput::avif
        ? media::OutputFormat::avif
        : media::OutputFormat::webp;
}

} // namespace

http::HttpResponse handle_function_url_event(
    std::span<const std::byte> event,
    http::OriginTimeoutApi timeout,
    http::AddressResolverApi resolver,
    http::OriginTransportApi transport)
{
    const std::string_view payload{
        reinterpret_cast<const char*>(event.data()), event.size()};
    http::RequestPlan plan = http::plan_request(
        http::parse_function_url_event(payload));
    if (std::holds_alternative<http::HttpResponse>(plan)) {
        return std::get<http::HttpResponse>(std::move(plan));
    }

    http::MediaRequest request =
        std::get<http::MediaRequest>(std::move(plan));
    http::UrlPolicyResult origin =
        http::validate_origin_url(request.source_url);
    if (!origin) {
        return http::make_error_response(http::ErrorResponse::access_denied);
    }

    http::OriginDownloadResult downloaded = http::download_origin(
        *origin.url, timeout, resolver, transport);
    if (!downloaded) {
        return http::make_error_response(http::ErrorResponse::internal);
    }

    const std::span<const std::byte> source{downloaded.response.body()};
    media::MediaConversionResult converted = media::convert_media(source,
        media::sniff_mime(source), request.options.force_static,
        preferred_output(request.options.preferred_output),
        media::ImageDimensions{
            .width = request.options.width_limit,
            .height = request.options.height_limit,
        });
    if (!converted) {
        return http::make_error_response(http::ErrorResponse::invalid_image);
    }
    return http::make_media_response(
        request.options.preferred_output, std::move(converted.body));
}

} // namespace mediaproxy
