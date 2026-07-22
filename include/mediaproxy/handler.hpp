#pragma once

#include <cstddef>
#include <cstdint>
#include <span>

#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/origin_download.hpp>
#include <mediaproxy/http/response.hpp>
#include <mediaproxy/media/conversion.hpp>

namespace mediaproxy {

enum class HandlerOutcome {
    status,
    bad_request,
    access_denied,
    origin_failure,
    media_failure,
    media_success,
};

struct HandlerDiagnostics {
    HandlerOutcome outcome = HandlerOutcome::bad_request;
    http::OriginDownloadError origin_error = http::OriginDownloadError::none;
    media::MediaConversionError media_error =
        media::MediaConversionError::none;
    std::size_t origin_bytes = 0;
    std::uint64_t fetch_microseconds = 0;
    std::uint64_t media_microseconds = 0;
};

[[nodiscard]] http::HttpResponse handle_function_url_event(
    std::span<const std::byte> event,
    http::OriginTimeoutApi timeout,
    http::AddressResolverApi resolver = {},
    http::OriginTransportApi transport = http::system_origin_transport(),
    HandlerDiagnostics* diagnostics = nullptr);

} // namespace mediaproxy
