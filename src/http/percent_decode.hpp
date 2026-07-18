#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace mediaproxy::http::detail {

[[nodiscard]] inline std::optional<unsigned char> decode_hex_pair(
    char high,
    char low) noexcept
{
    const auto decode_nibble = [](char value) -> std::optional<unsigned char> {
        if (value >= '0' && value <= '9') {
            return static_cast<unsigned char>(value - '0');
        }
        if (value >= 'a' && value <= 'f') {
            return static_cast<unsigned char>(value - 'a' + 10);
        }
        if (value >= 'A' && value <= 'F') {
            return static_cast<unsigned char>(value - 'A' + 10);
        }
        return std::nullopt;
    };

    const auto high_nibble = decode_nibble(high);
    const auto low_nibble = decode_nibble(low);
    if (!high_nibble || !low_nibble) {
        return std::nullopt;
    }
    return static_cast<unsigned char>((*high_nibble << 4U) | *low_nibble);
}

[[nodiscard]] inline std::optional<std::string> percent_decode(
    std::string_view encoded,
    bool plus_as_space)
{
    std::string decoded;
    decoded.reserve(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const char value = encoded[index];
        if (value == '+' && plus_as_space) {
            decoded.push_back(' ');
            continue;
        }
        if (value != '%') {
            decoded.push_back(value);
            continue;
        }
        if (encoded.size() - index < 3) {
            return std::nullopt;
        }
        const auto byte = decode_hex_pair(encoded[index + 1], encoded[index + 2]);
        if (!byte) {
            return std::nullopt;
        }
        decoded.push_back(static_cast<char>(*byte));
        index += 2;
    }
    return decoded;
}

} // namespace mediaproxy::http::detail
