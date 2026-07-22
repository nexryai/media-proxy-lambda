#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <string_view>

#include <mediaproxy/http/response.hpp>

namespace mediaproxy::runtime {

class ByteSink {
public:
    virtual ~ByteSink() = default;
    [[nodiscard]] virtual bool write(
        std::span<const std::byte> bytes) = 0;
};

inline constexpr std::size_t response_chunk_bytes = 64U * 1024U;

[[nodiscard]] std::string make_streaming_request_head(
    std::string_view runtime_authority,
    std::string_view request_id);

[[nodiscard]] bool write_streaming_response(
    ByteSink& sink,
    const http::HttpResponse& response);

[[nodiscard]] bool write_streaming_error(
    ByteSink& sink,
    std::string_view error_type,
    std::span<const std::byte> error_body);

} // namespace mediaproxy::runtime
