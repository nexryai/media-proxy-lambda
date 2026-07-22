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

[[nodiscard]] std::vector<std::byte> response_bytes(std::string_view text)
{
    const auto bytes = std::as_bytes(std::span{text});
    return {bytes.begin(), bytes.end()};
}

[[nodiscard]] HttpResponse text_response(
    std::uint16_t status,
    std::string_view body)
{
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

HttpResponse make_status_response()
{
    std::vector<HttpHeader> headers;
    headers.push_back({.name = "Content-Type", .value = "application/json"});
    return {
        .status = 200,
        .headers = std::move(headers),
        .body = response_bytes(R"({"status":"OK"})"),
    };
}

HttpResponse make_error_response(ErrorResponse error)
{
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

HttpResponse make_media_response(
    PreferredOutput output,
    std::vector<std::byte> body)
{
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
