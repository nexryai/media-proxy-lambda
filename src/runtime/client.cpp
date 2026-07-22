#include <mediaproxy/runtime/client.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <mediaproxy/runtime/streaming.hpp>

namespace mediaproxy::runtime {
namespace {

[[nodiscard]] bool write_text(
    SocketTransport& transport,
    std::string_view text)
{
    return transport.write(std::as_bytes(std::span{text}));
}

[[nodiscard]] bool read_response_ack(SocketTransport& transport)
{
    std::string headers;
    headers.reserve(1024);
    std::array<std::byte, 1024> buffer{};
    while (headers.size() <= maximum_runtime_header_bytes) {
        const std::ptrdiff_t received = transport.read_some(buffer);
        if (received <= 0) {
            return false;
        }
        const auto* characters =
            reinterpret_cast<const char*>(buffer.data());
        headers.append(characters, static_cast<std::size_t>(received));
        const std::size_t header_end = headers.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            return headers.substr(0, headers.find("\r\n"))
                == "HTTP/1.1 202 Accepted";
        }
    }
    return false;
}

} // namespace

std::optional<Invocation> poll_next_on(
    SocketTransport& transport,
    std::string_view runtime_authority)
{
    const std::string request = make_next_request_head(runtime_authority);
    if (request.empty() || !write_text(transport, request)) {
        return std::nullopt;
    }

    NextResponseParser parser;
    std::array<std::byte, 16U * 1024U> buffer{};
    while (parser.status() == NextParseStatus::incomplete) {
        const std::ptrdiff_t received = transport.read_some(buffer);
        if (received <= 0) {
            return std::nullopt;
        }
        const auto status = parser.feed(std::span{
            buffer.data(), static_cast<std::size_t>(received)});
        if (status == NextParseStatus::error) {
            return std::nullopt;
        }
    }
    return parser.invocation();
}

bool send_response_on(
    SocketTransport& transport,
    std::string_view runtime_authority,
    std::string_view request_id,
    const http::HttpResponse& response)
{
    const std::string head =
        make_streaming_request_head(runtime_authority, request_id);
    if (head.empty() || !write_text(transport, head)
        || !write_streaming_response(transport, response)
        || !transport.shutdown_write()) {
        return false;
    }
    return read_response_ack(transport);
}

RuntimeClient::RuntimeClient(std::string authority)
    : authority_(std::move(authority))
{
}

std::optional<Invocation> RuntimeClient::poll_next() const
{
    auto transport = SocketTransport::connect(authority_);
    if (!transport.has_value()) {
        return std::nullopt;
    }
    return poll_next_on(*transport, authority_);
}

bool RuntimeClient::send_response(
    std::string_view request_id,
    const http::HttpResponse& response) const
{
    auto transport = SocketTransport::connect(authority_);
    return transport.has_value()
        && send_response_on(*transport, authority_, request_id, response);
}

} // namespace mediaproxy::runtime
