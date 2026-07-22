#include <mediaproxy/runtime/next_response.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>

namespace mediaproxy::runtime {
namespace {

[[nodiscard]] char ascii_lower(char value) noexcept
{
    return value >= 'A' && value <= 'Z'
        ? static_cast<char>(value - 'A' + 'a')
        : value;
}

[[nodiscard]] bool ascii_equal(
    std::string_view left,
    std::string_view right) noexcept
{
    return left.size() == right.size()
        && std::equal(left.begin(), left.end(), right.begin(),
            [](char first, char second) {
                return ascii_lower(first) == ascii_lower(second);
            });
}

[[nodiscard]] std::string_view trim(std::string_view value) noexcept
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

template <typename Integer>
[[nodiscard]] bool parse_decimal(std::string_view value, Integer& output) noexcept
{
    if (value.empty()) {
        return false;
    }
    Integer parsed = 0;
    for (const char character : value) {
        if (character < '0' || character > '9') {
            return false;
        }
        const Integer digit = static_cast<Integer>(character - '0');
        if (parsed > (std::numeric_limits<Integer>::max() - digit) / 10) {
            return false;
        }
        parsed = static_cast<Integer>(parsed * 10 + digit);
    }
    output = parsed;
    return true;
}

[[nodiscard]] std::size_t find_header_end(
    std::span<const std::byte> bytes) noexcept
{
    constexpr std::string_view delimiter = "\r\n\r\n";
    if (bytes.size() < delimiter.size()) {
        return std::string_view::npos;
    }
    const auto* characters = reinterpret_cast<const char*>(bytes.data());
    const std::string_view text{characters, bytes.size()};
    return text.find(delimiter);
}

[[nodiscard]] bool safe_authority(std::string_view value) noexcept
{
    return !value.empty()
        && value.find_first_of("\r\n \t") == std::string_view::npos;
}

[[nodiscard]] bool safe_request_id(std::string_view value) noexcept
{
    return !value.empty()
        && value.find_first_of("/ ?#\t\\") == std::string_view::npos;
}

} // namespace

NextParseStatus NextResponseParser::feed(std::span<const std::byte> bytes)
{
    if (status_ != NextParseStatus::incomplete) {
        return status_;
    }
    if (headers_complete_) {
        return append_event(bytes);
    }

    const std::size_t available_header_bytes =
        maximum_runtime_header_bytes - buffer_.size();
    const std::size_t buffered =
        std::min(bytes.size(), available_header_bytes);
    buffer_.insert(buffer_.end(), bytes.begin(),
        bytes.begin() + static_cast<std::ptrdiff_t>(buffered));

    const std::size_t header_end = find_header_end(buffer_);
    if (header_end == std::string_view::npos) {
        if (buffered != bytes.size()
            || buffer_.size() == maximum_runtime_header_bytes) {
            status_ = NextParseStatus::error;
        }
        return status_;
    }
    constexpr std::size_t delimiter_size = 4;
    const std::size_t body_offset = header_end + delimiter_size;
    if (body_offset > maximum_runtime_header_bytes
        || !parse_headers(header_end)) {
        status_ = NextParseStatus::error;
        return status_;
    }
    headers_complete_ = true;
    invocation_.event.reserve(content_length_);
    const NextParseStatus buffered_status = append_event(
        std::span<const std::byte>{buffer_}.subspan(body_offset));
    if (buffered_status == NextParseStatus::error) {
        return status_;
    }
    buffer_.clear();
    return append_event(bytes.subspan(buffered));
}

NextParseStatus NextResponseParser::append_event(
    std::span<const std::byte> bytes)
{
    if (bytes.size() > content_length_ - invocation_.event.size()) {
        status_ = NextParseStatus::error;
        return status_;
    }
    invocation_.event.insert(
        invocation_.event.end(), bytes.begin(), bytes.end());
    if (invocation_.event.size() == content_length_) {
        status_ = NextParseStatus::complete;
    }
    return status_;
}

bool NextResponseParser::parse_headers(std::size_t header_end)
{
    const auto* characters = reinterpret_cast<const char*>(buffer_.data());
    const std::string_view headers{characters, header_end};
    const std::size_t first_end = headers.find("\r\n");
    if (first_end == std::string_view::npos
        || headers.substr(0, first_end) != "HTTP/1.1 200 OK") {
        return false;
    }

    bool have_length = false;
    bool have_request_id = false;
    bool have_deadline = false;
    bool have_trace = false;
    std::size_t line_begin = first_end + 2;
    while (line_begin < headers.size()) {
        const std::size_t line_end = headers.find("\r\n", line_begin);
        const std::size_t end = line_end == std::string_view::npos
            ? headers.size()
            : line_end;
        const std::string_view line = headers.substr(line_begin, end - line_begin);
        const std::size_t colon = line.find(':');
        if (colon == std::string_view::npos || colon == 0) {
            return false;
        }
        const std::string_view name = line.substr(0, colon);
        const std::string_view value = trim(line.substr(colon + 1));
        if (ascii_equal(name, "Content-Length")) {
            if (have_length || !parse_decimal(value, content_length_)
                || content_length_ > maximum_runtime_event_bytes) {
                return false;
            }
            have_length = true;
        } else if (ascii_equal(name, "Lambda-Runtime-Aws-Request-Id")) {
            if (have_request_id || !safe_request_id(value)) {
                return false;
            }
            invocation_.request_id = value;
            have_request_id = true;
        } else if (ascii_equal(name, "Lambda-Runtime-Deadline-Ms")) {
            if (have_deadline
                || !parse_decimal(value, invocation_.deadline_ms)) {
                return false;
            }
            have_deadline = true;
        } else if (ascii_equal(name, "Lambda-Runtime-Trace-Id")) {
            if (have_trace) {
                return false;
            }
            invocation_.trace_id = value;
            have_trace = true;
        } else if (ascii_equal(name, "Transfer-Encoding")) {
            return false;
        }
        line_begin = end + 2;
    }
    return have_length && have_request_id && have_deadline && have_trace;
}

std::string make_next_request_head(std::string_view runtime_authority)
{
    if (!safe_authority(runtime_authority)) {
        return {};
    }
    std::string output =
        "GET /2018-06-01/runtime/invocation/next HTTP/1.1\r\nHost: ";
    output += runtime_authority;
    output += "\r\nConnection: close\r\n\r\n";
    return output;
}

} // namespace mediaproxy::runtime
