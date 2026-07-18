#include <mediaproxy/http/query.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mediaproxy::http {
namespace {

[[nodiscard]] std::optional<unsigned char> decode_hex_pair(
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

[[nodiscard]] std::optional<std::string> query_unescape(
    std::string_view encoded)
{
    std::string decoded;
    decoded.reserve(encoded.size());
    for (std::size_t index = 0; index < encoded.size(); ++index) {
        const char value = encoded[index];
        if (value == '+') {
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

} // namespace

QueryParameters::QueryParameters(std::vector<QueryParameter> parameters)
    : parameters_(std::move(parameters))
{
}

const std::vector<QueryParameter>& QueryParameters::entries() const noexcept
{
    return parameters_;
}

std::string_view QueryParameters::first(std::string_view key) const noexcept
{
    for (const auto& parameter : parameters_) {
        if (parameter.key == key) {
            return parameter.value;
        }
    }
    return {};
}

bool QueryParameters::boolean(std::string_view key) const noexcept
{
    return first(key) == "1";
}

QueryParameters parse_query(std::string_view raw_query)
{
    std::vector<QueryParameter> parameters;
    std::size_t field_start = 0;
    while (field_start <= raw_query.size()) {
        std::size_t field_end = raw_query.find('&', field_start);
        if (field_end == std::string_view::npos) {
            field_end = raw_query.size();
        }

        const std::string_view field =
            raw_query.substr(field_start, field_end - field_start);
        if (!field.empty() && field.find(';') == std::string_view::npos) {
            const std::size_t equals = field.find('=');
            const std::string_view encoded_key = field.substr(0, equals);
            const std::string_view encoded_value = equals == std::string_view::npos
                ? std::string_view{}
                : field.substr(equals + 1);
            auto key = query_unescape(encoded_key);
            auto value = query_unescape(encoded_value);
            if (key && value) {
                parameters.push_back(
                    {.key = std::move(*key), .value = std::move(*value)});
            }
        }

        if (field_end == raw_query.size()) {
            break;
        }
        field_start = field_end + 1;
    }
    return QueryParameters{std::move(parameters)};
}

MediaOptions select_media_options(const QueryParameters& parameters) noexcept
{
    MediaOptions options;
    if (parameters.boolean("avatar")) {
        options = {
            .selector = MediaSelector::avatar,
            .width_limit = 320,
            .height_limit = 320,
            .preferred_output = PreferredOutput::avif,
        };
    } else if (parameters.boolean("emoji")) {
        options = {
            .selector = MediaSelector::emoji,
            .width_limit = 700,
            .height_limit = 128,
            .preferred_output = PreferredOutput::avif,
        };
    } else if (parameters.boolean("preview")) {
        options = {
            .selector = MediaSelector::preview,
            .width_limit = 200,
            .height_limit = 200,
            .preferred_output = PreferredOutput::webp,
        };
    } else if (parameters.boolean("badge")) {
        options = {
            .selector = MediaSelector::badge,
            .width_limit = 96,
            .height_limit = 96,
            .preferred_output = PreferredOutput::avif,
        };
    } else if (parameters.boolean("thumbnail")) {
        options = {
            .selector = MediaSelector::thumbnail,
            .width_limit = 500,
            .height_limit = 400,
            .preferred_output = PreferredOutput::webp,
        };
    } else if (parameters.boolean("ticker")) {
        options = {
            .selector = MediaSelector::ticker,
            .width_limit = 64,
            .height_limit = 64,
            .preferred_output = PreferredOutput::avif,
        };
    }
    options.force_static = parameters.boolean("static");
    return options;
}

} // namespace mediaproxy::http
