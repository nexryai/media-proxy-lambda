#include <mediaproxy/http/origin_response.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace mediaproxy::http {
namespace {

[[nodiscard]] std::string_view remove_line_ending(
    std::string_view line) noexcept
{
    if (line.ends_with("\r\n")) {
        line.remove_suffix(2);
    } else if (line.ends_with('\n')) {
        line.remove_suffix(1);
    }
    return line;
}

[[nodiscard]] bool ascii_iequals(
    std::string_view left,
    std::string_view right) noexcept
{
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto lowercase = [](char value) noexcept {
            return value >= 'A' && value <= 'Z'
                ? static_cast<char>(value - 'A' + 'a')
                : value;
        };
        if (lowercase(left[index]) != lowercase(right[index])) {
            return false;
        }
    }
    return true;
}

[[nodiscard]] std::string_view trim_optional_whitespace(
    std::string_view value) noexcept
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }
    return value;
}

} // namespace

void OriginResponseAccumulator::consume_header_line(
    std::string_view line) noexcept
{
    if (error_ != OriginResponseError::none) {
        return;
    }
    line = remove_line_ending(line);
    if (line == "Blocked-By: NextDNS") {
        error_ = OriginResponseError::blocked_by_nextdns;
        return;
    }

    const std::size_t separator = line.find(':');
    if (separator == std::string_view::npos
        || !ascii_iequals(line.substr(0, separator), "Content-Length")) {
        return;
    }
    set_content_length(trim_optional_whitespace(line.substr(separator + 1)));
}

void OriginResponseAccumulator::set_content_length(
    std::string_view value) noexcept
{
    if (value.empty()) {
        error_ = OriginResponseError::invalid_content_length;
        return;
    }

    if (value.front() == '+') {
        value.remove_prefix(1);
        if (value.empty()) {
            error_ = OriginResponseError::invalid_content_length;
            return;
        }
    }
    std::int64_t parsed = 0;
    const auto result = std::from_chars(
        value.data(), value.data() + value.size(), parsed, 10);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        error_ = OriginResponseError::invalid_content_length;
        return;
    }
    if (parsed > static_cast<std::int64_t>(maximum_origin_body_bytes)) {
        error_ = OriginResponseError::content_length_too_large;
        return;
    }
    content_length_ = parsed;
    if (parsed <= 0) {
        return;
    }
    try {
        body_.reserve(static_cast<std::size_t>(parsed));
    } catch (const std::bad_alloc&) {
        error_ = OriginResponseError::allocation;
    } catch (const std::length_error&) {
        error_ = OriginResponseError::allocation;
    }
}

std::size_t OriginResponseAccumulator::append_body(
    std::span<const std::byte> bytes) noexcept
{
    if (error_ != OriginResponseError::none || bytes.empty()
        || body_.size() == maximum_origin_body_bytes) {
        return 0;
    }
    const std::size_t remaining = maximum_origin_body_bytes - body_.size();
    const std::size_t accepted = std::min(remaining, bytes.size());
    try {
        body_.insert(body_.end(), bytes.begin(), bytes.begin() + accepted);
    } catch (const std::bad_alloc&) {
        error_ = OriginResponseError::allocation;
        return 0;
    } catch (const std::length_error&) {
        error_ = OriginResponseError::allocation;
        return 0;
    }
    return accepted;
}

bool OriginResponseAccumulator::finish(long status) noexcept
{
    if (error_ != OriginResponseError::none) {
        return false;
    }
    if (status != 200) {
        error_ = OriginResponseError::non_200_status;
        return false;
    }
    return true;
}

OriginResponseError OriginResponseAccumulator::error() const noexcept
{
    return error_;
}

std::optional<std::int64_t> OriginResponseAccumulator::content_length()
    const noexcept
{
    return content_length_;
}

const std::vector<std::byte>& OriginResponseAccumulator::body() const noexcept
{
    return body_;
}

bool OriginResponseAccumulator::at_body_limit() const noexcept
{
    return body_.size() == maximum_origin_body_bytes;
}

} // namespace mediaproxy::http
