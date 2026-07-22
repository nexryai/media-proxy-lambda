#include <mediaproxy/handler.hpp>

#include <chrono>
#include <cstddef>
#include <cstdint>
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

[[nodiscard]] std::uint64_t elapsed_microseconds(
    std::chrono::steady_clock::time_point start) noexcept
{
    const auto elapsed = std::chrono::steady_clock::now() - start;
    const auto microseconds =
        std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count();
    return microseconds > 0 ? static_cast<std::uint64_t>(microseconds) : 0;
}

void record_outcome(
    HandlerDiagnostics* diagnostics,
    HandlerOutcome outcome) noexcept
{
    if (diagnostics != nullptr) {
        diagnostics->outcome = outcome;
    }
}

} // namespace

http::HttpResponse handle_function_url_event(
    std::span<const std::byte> event,
    http::OriginTimeoutApi timeout,
    http::AddressResolverApi resolver,
    http::OriginTransportApi transport,
    HandlerDiagnostics* diagnostics)
{
    if (diagnostics != nullptr) {
        *diagnostics = {};
    }
    const std::string_view payload{
        reinterpret_cast<const char*>(event.data()), event.size()};
    http::MediaRequest request;
    {
        const http::EventParseResult parsed =
            http::parse_function_url_event(payload);
        http::RequestPlan plan = http::plan_request(parsed);
        if (std::holds_alternative<http::HttpResponse>(plan)) {
            record_outcome(diagnostics,
                parsed.request
                        && parsed.request->route == http::RequestRoute::status
                    ? HandlerOutcome::status
                    : HandlerOutcome::bad_request);
            return std::get<http::HttpResponse>(std::move(plan));
        }
        request = std::get<http::MediaRequest>(std::move(plan));
    }
    http::UrlPolicyResult origin =
        http::validate_origin_url(request.source_url);
    if (!origin) {
        record_outcome(diagnostics, HandlerOutcome::access_denied);
        return http::make_error_response(http::ErrorResponse::access_denied);
    }

    const auto fetch_start = std::chrono::steady_clock::now();
    http::OriginDownloadResult downloaded = http::download_origin(
        *origin.url, timeout, resolver, transport);
    if (diagnostics != nullptr) {
        diagnostics->fetch_microseconds = elapsed_microseconds(fetch_start);
        diagnostics->origin_error = downloaded.error;
        diagnostics->origin_bytes = downloaded.response.body().size();
    }
    if (!downloaded) {
        record_outcome(diagnostics, HandlerOutcome::origin_failure);
        return http::make_error_response(http::ErrorResponse::internal);
    }

    const std::span<const std::byte> source{downloaded.response.body()};
    const auto media_start = std::chrono::steady_clock::now();
    media::MediaConversionResult converted = media::convert_media(source,
        media::sniff_mime(source), request.options.force_static,
        preferred_output(request.options.preferred_output),
        media::ImageDimensions{
            .width = request.options.width_limit,
            .height = request.options.height_limit,
        });
    if (diagnostics != nullptr) {
        diagnostics->media_microseconds = elapsed_microseconds(media_start);
        diagnostics->media_error = converted.error;
    }
    if (!converted) {
        record_outcome(diagnostics, HandlerOutcome::media_failure);
        return http::make_error_response(http::ErrorResponse::invalid_image);
    }
    record_outcome(diagnostics, HandlerOutcome::media_success);
    return http::make_media_response(
        request.options.preferred_output, std::move(converted.body));
}

} // namespace mediaproxy
