#include <mediaproxy/http/query.hpp>

#include "percent_decode.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace mediaproxy::http {

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
            auto key = detail::percent_decode(encoded_key, true);
            auto value = detail::percent_decode(encoded_value, true);
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
