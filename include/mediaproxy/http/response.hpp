#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <mediaproxy/http/query.hpp>

namespace mediaproxy::http {

struct HttpHeader {
    std::string name;
    std::string value;
};

struct HttpResponse {
    std::uint16_t status = 500;
    std::vector<HttpHeader> headers;
    std::vector<std::byte> body;
};

enum class ErrorResponse {
    bad_request,
    access_denied,
    invalid_image,
    internal,
};

[[nodiscard]] HttpResponse make_status_response();
[[nodiscard]] HttpResponse make_error_response(ErrorResponse error);
[[nodiscard]] HttpResponse make_media_response(
    PreferredOutput output,
    std::vector<std::byte> body);

} // namespace mediaproxy::http
