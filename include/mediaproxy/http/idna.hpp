#pragma once

#include <string>
#include <string_view>

namespace mediaproxy::http {

enum class HostnameError {
    none,
    empty,
    input_too_long,
    idna_conversion,
    forbidden_code_point,
    empty_label,
    invalid_ascii_label,
    invalid_hyphen,
    label_too_long,
    hostname_too_long,
};

struct HostnameNormalization {
    std::string ascii;
    HostnameError error = HostnameError::none;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == HostnameError::none;
    }
};

[[nodiscard]] HostnameNormalization normalize_hostname(
    std::string_view hostname);

} // namespace mediaproxy::http
