#include <mediaproxy/media/encoding_quality.hpp>

namespace mediaproxy::media {
namespace {

constexpr int standard_quality = 65;
constexpr int original_size_quality = 70;

} // namespace

int encoding_quality_value(EncodingQuality quality) noexcept
{
    return quality == EncodingQuality::url_only
        ? original_size_quality
        : standard_quality;
}

} // namespace mediaproxy::media
