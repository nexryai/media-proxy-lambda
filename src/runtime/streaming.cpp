#include <mediaproxy/runtime/streaming.hpp>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>

namespace mediaproxy::runtime {
namespace {

[[nodiscard]] auto safe_header_component(std::string_view value) noexcept -> bool {

    return !value.empty() && value.find_first_of("\r\n") == std::string_view::npos;
}

void append_json_string(std::string &output, std::string_view value) {
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
            case '\b':
                output += "\\b";
                break;
            case '\f':
                output += "\\f";
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

[[nodiscard]] auto integration_metadata(const http::HttpResponse &response) -> std::string {
    std::string output = "{\"statusCode\":";
    output += std::to_string(response.status);
    output += ",\"headers\":{";
    bool first = true;
    for (const auto &header : response.headers) {
        if (!first) {
            output.push_back(',');
        }
        first = false;
        append_json_string(output, header.name);
        output.push_back(':');
        append_json_string(output, header.value);
    }
    output += "}}";

    return output;
}

[[nodiscard]] auto chunk_prefix(std::size_t size) -> std::string {
    constexpr char hex[] = "0123456789abcdef";
    std::array<char, sizeof(std::size_t) * 2> reversed{};
    std::size_t count = 0;
    do {
        reversed[count++] = hex[size & 0x0fU];
        size >>= 4U;
    } while (size != 0);

    std::string output;
    output.reserve(count + 2);
    while (count > 0) {
        output.push_back(reversed[--count]);
    }
    output += "\r\n";

    return output;
}

[[nodiscard]] auto write_text(ByteSink &sink, std::string_view text) -> bool {

    return sink.write(std::as_bytes(std::span{text}));
}

[[nodiscard]] auto write_chunk(ByteSink &sink, std::span<const std::byte> bytes) -> bool {
    if (bytes.empty()) {
        return true;
    }
    const std::string prefix = chunk_prefix(bytes.size());

    return write_text(sink, prefix) && sink.write(bytes) && write_text(sink, "\r\n");
}

[[nodiscard]] auto base64(std::span<const std::byte> input) -> std::string {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string output;
    output.reserve(((input.size() + 2) / 3) * 4);
    for (std::size_t offset = 0; offset < input.size(); offset += 3) {
        const std::uint32_t first = std::to_integer<std::uint8_t>(input[offset]);
        const std::uint32_t second = offset + 1 < input.size() ? std::to_integer<std::uint8_t>(input[offset + 1]) : 0;
        const std::uint32_t third = offset + 2 < input.size() ? std::to_integer<std::uint8_t>(input[offset + 2]) : 0;
        const std::uint32_t value = (first << 16U) | (second << 8U) | third;
        output.push_back(alphabet[(value >> 18U) & 0x3fU]);
        output.push_back(alphabet[(value >> 12U) & 0x3fU]);
        output.push_back(offset + 1 < input.size() ? alphabet[(value >> 6U) & 0x3fU] : '=');
        output.push_back(offset + 2 < input.size() ? alphabet[value & 0x3fU] : '=');
    }

    return output;
}

} // namespace

auto make_streaming_request_head(std::string_view runtime_authority, std::string_view request_id) -> std::string {
    if (!safe_header_component(runtime_authority) || !safe_header_component(request_id) || request_id.find_first_of("/ ?#\t\\") != std::string_view::npos) {
        return {};
    }
    std::string output = "POST /2018-06-01/runtime/invocation/";
    output += request_id;
    output += "/response HTTP/1.1\r\nHost: ";
    output += runtime_authority;
    output += "\r\nLambda-Runtime-Function-Response-Mode: streaming"
              "\r\nTransfer-Encoding: chunked"
              "\r\nContent-Type: application/vnd.awslambda.http-integration-response"
              "\r\nTrailer: Lambda-Runtime-Function-Error-Type, "
              "Lambda-Runtime-Function-Error-Body"
              "\r\nConnection: close\r\n\r\n";

    return output;
}

auto write_streaming_response(ByteSink &sink, const http::HttpResponse &response) -> bool {
    std::string metadata = integration_metadata(response);
    metadata.append(8, '\0');
    if (!write_chunk(sink, std::as_bytes(std::span{metadata}))) {
        return false;
    }

    const std::span<const std::byte> body{response.body};
    for (std::size_t offset = 0; offset < body.size();) {
        const std::size_t size = std::min(response_chunk_bytes, body.size() - offset);
        if (!write_chunk(sink, body.subspan(offset, size))) {
            return false;
        }
        offset += size;
    }

    return write_text(sink, "0\r\n\r\n");
}

auto write_streaming_error(ByteSink &sink, std::string_view error_type, std::span<const std::byte> error_body) -> bool {
    if (!safe_header_component(error_type)) {
        return false;
    }
    const std::string encoded = base64(error_body);

    return write_text(sink, "0\r\nLambda-Runtime-Function-Error-Type: ") && write_text(sink, error_type) && write_text(sink, "\r\nLambda-Runtime-Function-Error-Body: ") && write_text(sink, encoded) && write_text(sink, "\r\n\r\n");
}

} // namespace mediaproxy::runtime
