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
    [[nodiscard]] virtual auto write(std::span<const std::byte> bytes) -> bool = 0;
};

inline constexpr std::size_t response_chunk_bytes = 64U * 1024U;

[[nodiscard]] auto make_streaming_request_head(std::string_view runtime_authority, std::string_view request_id) -> std::string;

[[nodiscard]] auto write_streaming_response(ByteSink &sink, const http::HttpResponse &response) -> bool;

[[nodiscard]] auto write_streaming_error(ByteSink &sink, std::string_view error_type, std::span<const std::byte> error_body) -> bool;

} // namespace mediaproxy::runtime
