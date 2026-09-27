#include <mediaproxy/http/response.hpp>

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mediaproxy::http {
namespace {

[[nodiscard]] auto response_bytes(std::string_view text) -> std::vector<std::byte> {
    const auto bytes = std::as_bytes(std::span{text});

    return {bytes.begin(), bytes.end()};
}

[[nodiscard]] auto text_response(std::uint16_t status, std::string_view body) -> HttpResponse {
    std::vector<HttpHeader> headers;
    headers.push_back({
        .name = "Content-Type",
        .value = "text/plain; charset=utf-8",
    });

    return {
        .status = status,
        .headers = std::move(headers),
        .body = response_bytes(body),
    };
}

} // namespace

auto make_status_response() -> HttpResponse {
    std::vector<HttpHeader> headers;
    headers.push_back({.name = "Content-Type", .value = "application/json"});

    return {
        .status = 200,
        .headers = std::move(headers),
        .body = response_bytes(R"({"status":"OK"})"),
    };
}

auto make_error_response(ErrorResponse error) -> HttpResponse {
    switch (error) {
        case ErrorResponse::bad_request:
            return text_response(400, "Bad request\n");
        case ErrorResponse::access_denied:
            return text_response(403, "Access denied\n");
        case ErrorResponse::invalid_image:
            return text_response(400, "Failed to resize image: invalid image?\n");
        case ErrorResponse::internal:
            return text_response(500, "Internal Server Error\n");
    }
    __builtin_unreachable();
}

auto make_media_response(PreferredOutput output, std::vector<std::byte> body) -> HttpResponse {
    std::vector<HttpHeader> headers;
    headers.reserve(3);
    headers.push_back({
        .name = "Content-Type",
        .value = output == PreferredOutput::avif ? "image/avif" : "image/webp",
    });
    headers.push_back({
        .name = "CDN-Cache-Control",
        .value = "max-age=604800",
    });
    headers.push_back({
        .name = "Cache-Control",
        .value = "max-age=432000",
    });

    return {
        .status = 200,
        .headers = std::move(headers),
        .body = std::move(body),
    };
}

} // namespace mediaproxy::http
