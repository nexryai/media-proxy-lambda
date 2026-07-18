#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mediaproxy::http {

struct QueryParameter {
    std::string key;
    std::string value;
};

class QueryParameters {
public:
    explicit QueryParameters(std::vector<QueryParameter> parameters);

    [[nodiscard]] const std::vector<QueryParameter>& entries() const noexcept;
    [[nodiscard]] std::string_view first(std::string_view key) const noexcept;
    [[nodiscard]] bool boolean(std::string_view key) const noexcept;

private:
    std::vector<QueryParameter> parameters_;
};

[[nodiscard]] QueryParameters parse_query(std::string_view raw_query);

enum class MediaSelector {
    avatar,
    emoji,
    preview,
    badge,
    thumbnail,
    ticker,
    default_media,
};

enum class PreferredOutput {
    avif,
    webp,
};

struct MediaOptions {
    MediaSelector selector = MediaSelector::default_media;
    std::uint32_t width_limit = 3200;
    std::uint32_t height_limit = 3200;
    PreferredOutput preferred_output = PreferredOutput::webp;
    bool force_static = false;
};

[[nodiscard]] MediaOptions select_media_options(
    const QueryParameters& parameters) noexcept;

} // namespace mediaproxy::http
