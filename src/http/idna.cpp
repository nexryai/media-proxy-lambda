#include <mediaproxy/http/idna.hpp>

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>

#include <ada/ada_idna.h>

namespace mediaproxy::http {
namespace {

constexpr std::size_t max_hostname_input_bytes = 4096;
constexpr std::size_t max_dns_label_bytes = 63;
constexpr std::size_t max_dns_hostname_bytes = 253;
constexpr std::size_t max_dns_hostname_with_trailing_dot_bytes = 254;

[[nodiscard]] bool is_ascii_letter_or_digit(char value) noexcept
{
    return (value >= 'a' && value <= 'z')
        || (value >= '0' && value <= '9');
}

[[nodiscard]] HostnameNormalization fail(HostnameError error)
{
    return {.ascii = {}, .error = error};
}

} // namespace

HostnameNormalization normalize_hostname(std::string_view hostname)
{
    if (hostname.empty()) {
        return fail(HostnameError::empty);
    }
    if (hostname.size() > max_hostname_input_bytes) {
        return fail(HostnameError::input_too_long);
    }

    std::string ascii = ada::idna::to_ascii(hostname);
    if (ascii.empty()) {
        return fail(HostnameError::idna_conversion);
    }
    if (ada::idna::contains_forbidden_domain_code_point(ascii)) {
        return fail(HostnameError::forbidden_code_point);
    }

    const bool has_trailing_dot = ascii.ends_with('.');
    const std::size_t maximum_size = has_trailing_dot
        ? max_dns_hostname_with_trailing_dot_bytes
        : max_dns_hostname_bytes;
    if (ascii.size() > maximum_size) {
        return fail(HostnameError::hostname_too_long);
    }

    const std::size_t labels_end =
        has_trailing_dot ? ascii.size() - 1 : ascii.size();
    if (labels_end == 0) {
        return fail(HostnameError::empty_label);
    }
    if (has_trailing_dot && ascii[labels_end - 1] == '.') {
        return fail(HostnameError::empty_label);
    }

    std::size_t label_start = 0;
    while (label_start < labels_end) {
        std::size_t label_end = ascii.find('.', label_start);
        if (label_end == std::string::npos || label_end > labels_end) {
            label_end = labels_end;
        }
        if (label_end == label_start) {
            return fail(HostnameError::empty_label);
        }

        const std::string_view label{
            ascii.data() + label_start, label_end - label_start};
        if (label.size() > max_dns_label_bytes) {
            return fail(HostnameError::label_too_long);
        }
        for (const char value : label) {
            if (!is_ascii_letter_or_digit(value) && value != '-') {
                return fail(HostnameError::invalid_ascii_label);
            }
        }
        if (label.front() == '-' || label.back() == '-') {
            return fail(HostnameError::invalid_hyphen);
        }
        if (label.size() >= 4
            && label[2] == '-'
            && label[3] == '-'
            && !label.starts_with("xn--")) {
            return fail(HostnameError::invalid_hyphen);
        }

        label_start = label_end + 1;
    }

    return {.ascii = std::move(ascii), .error = HostnameError::none};
}

} // namespace mediaproxy::http
