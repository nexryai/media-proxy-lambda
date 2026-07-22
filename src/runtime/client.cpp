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

[[nodiscard]] bool safe_line(std::string_view value) noexcept
{
    return !value.empty()
        && value.find_first_of("\r\n") == std::string_view::npos;
}

[[nodiscard]] bool safe_request_id(std::string_view value) noexcept
{
    return safe_line(value)
        && value.find_first_of("/?# \t\\") == std::string_view::npos;
}

void append_json_string(std::string& output, std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
        case '"':
            output += "\\\"";
            break;
        case '\\':
            output += "\\\\";
            break;
        case '\n':
            output += "\\n";
            break;
        case '\r':
            output += "\\r";
            break;
        case '\t':
            output += "\\t";
            break;
        default:
            if (character < 0x20U) {
                output += "\\u00";
                output.push_back(hex[character >> 4U]);
                output.push_back(hex[character & 0x0fU]);
            } else {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    output.push_back('"');
}

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
    return parser.take_invocation();
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

bool send_invocation_error_on(
    SocketTransport& transport,
    std::string_view runtime_authority,
    std::string_view request_id,
    std::string_view error_type,
    std::string_view error_message)
{
    if (!parse_runtime_authority(runtime_authority).has_value()
        || !safe_request_id(request_id) || !safe_line(error_type)) {
        return false;
    }
    std::string body = "{\"errorMessage\":";
    append_json_string(body, error_message);
    body += ",\"errorType\":";
    append_json_string(body, error_type);
    body.push_back('}');

    std::string request = "POST /2018-06-01/runtime/invocation/";
    request += request_id;
    request += "/error HTTP/1.1\r\nHost: ";
    request += runtime_authority;
    request += "\r\nLambda-Runtime-Function-Error-Type: ";
    request += error_type;
    request +=
        "\r\nContent-Type: application/vnd.aws.lambda.error+json"
        "\r\nContent-Length: ";
    request += std::to_string(body.size());
    request += "\r\nConnection: close\r\n\r\n";
    request += body;
    return write_text(transport, request) && transport.shutdown_write()
        && read_response_ack(transport);
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

bool RuntimeClient::send_invocation_error(
    std::string_view request_id,
    std::string_view error_type,
    std::string_view error_message) const
{
    auto transport = SocketTransport::connect(authority_);
    return transport.has_value()
        && send_invocation_error_on(*transport, authority_, request_id,
            error_type, error_message);
}

} // namespace mediaproxy::runtime
