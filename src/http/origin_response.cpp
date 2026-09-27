#include <mediaproxy/http/origin_response.hpp>

#include <algorithm>
#include <charconv>
#include <cstddef>
#include <cstdint>
#include <new>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace mediaproxy::http {
namespace {

[[nodiscard]] auto remove_line_ending(std::string_view line) noexcept -> std::string_view {
    if (line.ends_with("\r\n")) {
        line.remove_suffix(2);
    } else if (line.ends_with('\n')) {
        line.remove_suffix(1);
    }

    return line;
}

[[nodiscard]] auto ascii_iequals(std::string_view left, std::string_view right) noexcept -> bool {
    if (left.size() != right.size()) {
        return false;
    }
    for (std::size_t index = 0; index < left.size(); ++index) {
        const auto lowercase = [](char value) noexcept -> char { return value >= 'A' && value <= 'Z' ? static_cast<char>(value - 'A' + 'a') : value; };
        if (lowercase(left[index]) != lowercase(right[index])) {
            return false;
        }
    }

    return true;
}

[[nodiscard]] auto trim_optional_whitespace(std::string_view value) noexcept -> std::string_view {
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
        value.remove_prefix(1);
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t')) {
        value.remove_suffix(1);
    }

    return value;
}

} // namespace

void OriginResponseAccumulator::consume_header_line(std::string_view line) noexcept {
    if (error_ != OriginResponseError::none) {
        return;
    }
    line = remove_line_ending(line);
    if (line == "Blocked-By: NextDNS") {
        error_ = OriginResponseError::blocked_by_nextdns;
        return;
    }

    const std::size_t separator = line.find(':');
    if (separator == std::string_view::npos) {
        return;
    }
    const std::string_view name = line.substr(0, separator);
    const std::string_view value = trim_optional_whitespace(line.substr(separator + 1));
    if (ascii_iequals(name, "Content-Length")) {
        set_content_length(value);
    } else if (ascii_iequals(name, "Location")) {
        try {
            // Preserve the field value verbatim after HTTP optional whitespace
            // removal; URL resolution and policy validation happen later.
            location_ = std::string{value};
        } catch (const std::bad_alloc &) {
            error_ = OriginResponseError::allocation;
        } catch (const std::length_error &) {
            error_ = OriginResponseError::allocation;
        }
    }
}

void OriginResponseAccumulator::set_content_length(std::string_view value) noexcept {
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
    const auto result = std::from_chars(value.data(), value.data() + value.size(), parsed, 10);
    if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) {
        error_ = OriginResponseError::invalid_content_length;
        return;
    }
    if (std::cmp_greater(parsed, maximum_origin_body_bytes)) {
        error_ = OriginResponseError::content_length_too_large;
        return;
    }
    content_length_ = parsed;
    if (parsed <= 0) {
        return;
    }
    try {
        body_.reserve(static_cast<std::size_t>(parsed));
    } catch (const std::bad_alloc &) {
        error_ = OriginResponseError::allocation;
    } catch (const std::length_error &) {
        error_ = OriginResponseError::allocation;
    }
}

auto OriginResponseAccumulator::append_body(std::span<const std::byte> bytes) noexcept -> std::size_t {
    if (error_ != OriginResponseError::none || bytes.empty() || body_.size() == maximum_origin_body_bytes) {
        return 0;
    }
    const std::size_t remaining = maximum_origin_body_bytes - body_.size();
    const std::size_t accepted = std::min(remaining, bytes.size());
    try {
        body_.insert(body_.end(), bytes.begin(), bytes.begin() + accepted);
    } catch (const std::bad_alloc &) {
        error_ = OriginResponseError::allocation;
        return 0;
    } catch (const std::length_error &) {
        error_ = OriginResponseError::allocation;
        return 0;
    }

    return accepted;
}

auto OriginResponseAccumulator::finish(long status) noexcept -> bool {
    if (error_ != OriginResponseError::none) {
        return false;
    }
    if (status != 200) {
        error_ = OriginResponseError::non_200_status;
        return false;
    }

    return true;
}

auto OriginResponseAccumulator::error() const noexcept -> OriginResponseError {

    return error_;
}

auto OriginResponseAccumulator::content_length() const noexcept -> std::optional<std::int64_t> {

    return content_length_;
}

auto OriginResponseAccumulator::location() const noexcept -> const std::optional<std::string> & {

    return location_;
}

auto OriginResponseAccumulator::body() const noexcept -> const std::vector<std::byte> & {

    return body_;
}

auto OriginResponseAccumulator::at_body_limit() const noexcept -> bool {

    return body_.size() == maximum_origin_body_bytes;
}

} // namespace mediaproxy::http
