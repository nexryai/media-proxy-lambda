#include <mediaproxy/logging.hpp>

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string_view>

#include <mediaproxy/handler.hpp>
#include <mediaproxy/http/origin_download.hpp>
#include <mediaproxy/media/conversion.hpp>

namespace mediaproxy {
namespace {

void write_json_string(std::FILE* output, std::string_view value) noexcept
{
    constexpr char hex[] = "0123456789abcdef";
    std::fputc('"', output);
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            std::fputs("\\\"", output);
            break;
        case '\\':
            std::fputs("\\\\", output);
            break;
        case '\n':
            std::fputs("\\n", output);
            break;
        case '\r':
            std::fputs("\\r", output);
            break;
        case '\t':
            std::fputs("\\t", output);
            break;
        default:
            if (character < 0x20U) {
                const char escape[] = {'\\', 'u', '0', '0',
                    hex[character >> 4U], hex[character & 0x0fU], '\0'};
                std::fputs(escape, output);
            } else {
                std::fputc(character, output);
            }
            break;
        }
    }
    std::fputc('"', output);
}

[[nodiscard]] std::string_view outcome_name(HandlerOutcome outcome) noexcept
{
    switch (outcome) {
    case HandlerOutcome::status:
        return "status";
    case HandlerOutcome::bad_request:
        return "bad_request";
    case HandlerOutcome::access_denied:
        return "access_denied";
    case HandlerOutcome::origin_failure:
        return "origin_failure";
    case HandlerOutcome::media_failure:
        return "media_failure";
    case HandlerOutcome::media_success:
        return "media_success";
    }
    __builtin_unreachable();
}

[[nodiscard]] std::string_view origin_error_name(
    http::OriginDownloadError error) noexcept
{
    switch (error) {
    case http::OriginDownloadError::none:
        return "none";
    case http::OriginDownloadError::invalid_argument:
        return "invalid_argument";
    case http::OriginDownloadError::resolution:
        return "resolution";
    case http::OriginDownloadError::resolve_pin:
        return "resolve_pin";
    case http::OriginDownloadError::easy_init:
        return "easy_init";
    case http::OriginDownloadError::curl_config:
        return "curl_config";
    case http::OriginDownloadError::transfer:
        return "transfer";
    case http::OriginDownloadError::response_info:
        return "response_info";
    case http::OriginDownloadError::response_policy:
        return "response_policy";
    case http::OriginDownloadError::deadline:
        return "deadline";
    case http::OriginDownloadError::redirect:
        return "redirect";
    }
    __builtin_unreachable();
}

[[nodiscard]] std::string_view media_error_name(
    media::MediaConversionError error) noexcept
{
    switch (error) {
    case media::MediaConversionError::none:
        return "none";
    case media::MediaConversionError::unsupported:
        return "unsupported";
    case media::MediaConversionError::decode:
        return "decode";
    case media::MediaConversionError::convert:
        return "convert";
    }
    __builtin_unreachable();
}

} // namespace

void log_invocation(
    std::FILE* output,
    std::string_view request_id,
    const HandlerDiagnostics& diagnostics,
    std::uint16_t status,
    std::size_t event_bytes,
    std::size_t response_bytes,
    std::uint64_t handler_microseconds) noexcept
{
    if (output == nullptr) {
        return;
    }
    std::fputs("{\"category\":\"invocation\",\"requestId\":", output);
    write_json_string(output, request_id);
    std::fputs(",\"outcome\":", output);
    write_json_string(output, outcome_name(diagnostics.outcome));
    std::fputs(",\"originError\":", output);
    write_json_string(output, origin_error_name(diagnostics.origin_error));
    std::fputs(",\"mediaError\":", output);
    write_json_string(output, media_error_name(diagnostics.media_error));
    std::fprintf(output,
        ",\"status\":%u,\"eventBytes\":%zu,\"originBytes\":%zu"
        ",\"responseBytes\":%zu,\"fetchMicros\":%llu"
        ",\"mediaMicros\":%llu,\"handlerMicros\":%llu}\n",
        static_cast<unsigned int>(status), event_bytes,
        diagnostics.origin_bytes, response_bytes,
        static_cast<unsigned long long>(diagnostics.fetch_microseconds),
        static_cast<unsigned long long>(diagnostics.media_microseconds),
        static_cast<unsigned long long>(handler_microseconds));
}

void log_runtime_failure(
    std::FILE* output,
    std::string_view request_id,
    std::string_view category) noexcept
{
    if (output == nullptr) {
        return;
    }
    std::fputs("{\"category\":", output);
    write_json_string(output, category);
    std::fputs(",\"requestId\":", output);
    write_json_string(output, request_id);
    std::fputs("}\n", output);
}

} // namespace mediaproxy
