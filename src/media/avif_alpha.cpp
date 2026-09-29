#include "avif_alpha.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

namespace mediaproxy::media {
namespace {

struct TrackRelease {
    void operator()(heif_track *track) const noexcept {
        heif_track_release(track);
    }
};

struct ImageRelease {
    void operator()(heif_image *image) const noexcept {
        heif_image_release(image);
    }
};

using TrackPtr = std::unique_ptr<heif_track, TrackRelease>;
using ImagePtr = std::unique_ptr<heif_image, ImageRelease>;

} // namespace

auto find_avif_alpha_track(heif_context *context, heif_track *visual, heif_track **alpha) -> bool {
    *alpha = nullptr;
    if (heif_track_has_alpha_channel(visual) != 0) {
        return true;
    }

    // Some AVIF sequences label their alpha track "pict" instead of "auxv".
    // libheif does not attach those samples to the decoded visual frames.
    std::array<std::uint32_t, 2> ids{};
    const std::size_t count = heif_track_find_referring_tracks(visual, heif_track_reference_type_auxiliary, ids.data(), ids.size());
    if (count == 0) {
        return true;
    }
    if (count != 1) {
        return false;
    }

    TrackPtr candidate(heif_context_get_track(context, ids[0]));
    if (!candidate || heif_track_get_track_handler_type(candidate.get()) != heif_track_type_image_sequence || heif_track_get_timescale(candidate.get()) != heif_track_get_timescale(visual)) {
        return false;
    }

    *alpha = candidate.release();

    return true;
}

auto apply_avif_alpha_frame(heif_track *alpha, heif_image *rgba, const heif_decoding_options *options) -> bool {
    if (alpha == nullptr) {
        return true;
    }

    heif_image *raw_alpha = nullptr;
    const heif_error error = heif_track_decode_next_image(alpha, &raw_alpha, heif_colorspace_undefined, heif_chroma_undefined, options);
    ImagePtr alpha_image(raw_alpha);
    if (error.code != heif_error_Ok || !alpha_image || heif_image_get_chroma_format(alpha_image.get()) != heif_chroma_monochrome || heif_image_get_duration(alpha_image.get()) != heif_image_get_duration(rgba)) {
        return false;
    }

    const int width = heif_image_get_width(rgba, heif_channel_interleaved);
    const int height = heif_image_get_height(rgba, heif_channel_interleaved);
    if (width <= 0 || height <= 0 || heif_image_get_width(alpha_image.get(), heif_channel_Y) != width || heif_image_get_height(alpha_image.get(), heif_channel_Y) != height) {
        return false;
    }

    constexpr int byte_bits = 8;
    constexpr int word_bits = 16;
    constexpr std::size_t rgba_channels = 4;
    constexpr std::uint32_t maximum_alpha = 255;
    const int depth = heif_image_get_bits_per_pixel_range(alpha_image.get(), heif_channel_Y);
    const int storage_bits = heif_image_get_bits_per_pixel(alpha_image.get(), heif_channel_Y);
    if (depth < 1 || depth > word_bits || (storage_bits != byte_bits && storage_bits != word_bits) || depth > storage_bits) {
        return false;
    }

    const auto bytes_per_sample = static_cast<std::size_t>(storage_bits / byte_bits);
    const auto row_width = static_cast<std::size_t>(width);
    std::size_t alpha_stride = 0;
    std::size_t rgba_stride = 0;
    const std::uint8_t *alpha_pixels = heif_image_get_plane_readonly2(alpha_image.get(), heif_channel_Y, &alpha_stride);
    std::uint8_t *rgba_pixels = heif_image_get_plane2(rgba, heif_channel_interleaved, &rgba_stride);
    if ((alpha_pixels == nullptr) || (rgba_pixels == nullptr) || alpha_stride < row_width * bytes_per_sample || rgba_stride < row_width * rgba_channels || static_cast<std::size_t>(height) > std::numeric_limits<std::size_t>::max() / std::max(alpha_stride, rgba_stride)) {
        return false;
    }

    const std::uint32_t maximum = (1U << depth) - 1U;
    for (int y = 0; y < height; ++y) {
        const std::uint8_t *alpha_row = alpha_pixels + (static_cast<std::size_t>(y) * alpha_stride);
        std::uint8_t *rgba_row = rgba_pixels + (static_cast<std::size_t>(y) * rgba_stride);
        for (int x = 0; x < width; ++x) {
            std::uint32_t sample = alpha_row[static_cast<std::size_t>(x) * bytes_per_sample];
            if (storage_bits == word_bits) {
                std::uint16_t wide = 0;
                std::memcpy(&wide, alpha_row + (static_cast<std::size_t>(x) * bytes_per_sample), sizeof(wide));
                sample = wide;
            }
            if (sample > maximum) {
                return false;
            }
            rgba_row[(static_cast<std::size_t>(x) * rgba_channels) + 3] = static_cast<std::uint8_t>(((sample * maximum_alpha) + (maximum / 2U)) / maximum);
        }
    }

    return true;
}

} // namespace mediaproxy::media
