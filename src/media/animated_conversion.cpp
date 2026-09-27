#include <mediaproxy/media/animated_conversion.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

#include <glib.h>
#include <libheif/heif.h>
#include <libheif/heif_sequences.h>
#include <mediaproxy/media/vips_runtime.hpp>
#include <vips/vips.h>
#include <webp/encode.h>
#include <webp/mux.h>

namespace mediaproxy::media {
namespace {

struct ImageUnref {
    void operator()(VipsImage *image) const noexcept {
        if (image != nullptr) {
            g_object_unref(image);
        }
    }
};

struct GFree {
    void operator()(void *memory) const noexcept {
        g_free(memory);
    }
};

struct HeifContextFree {
    void operator()(heif_context *context) const noexcept {
        heif_context_free(context);
    }
};

struct HeifTrackRelease {
    void operator()(heif_track *track) const noexcept {
        heif_track_release(track);
    }
};

struct HeifImageRelease {
    void operator()(heif_image *image) const noexcept {
        heif_image_release(image);
    }
};

struct HeifDecodingOptionsFree {
    void operator()(heif_decoding_options *options) const noexcept {
        heif_decoding_options_free(options);
    }
};

struct EncoderDelete {
    void operator()(WebPAnimEncoder *encoder) const noexcept {
        WebPAnimEncoderDelete(encoder);
    }
};

struct Picture {
    WebPPicture value{};
    bool initialized = false;

    Picture()
        : initialized(WebPPictureInit(&value) != 0) {
    }

    ~Picture() {
        if (initialized) {
            WebPPictureFree(&value);
        }
    }

    Picture(const Picture &) = delete;
    Picture &operator=(const Picture &) = delete;
};

struct WebpData {
    WebPData value{};

    WebpData() {
        WebPDataInit(&value);
    }
    ~WebpData() {
        WebPDataClear(&value);
    }

    WebpData(const WebpData &) = delete;
    WebpData &operator=(const WebpData &) = delete;
};

using ImagePtr = std::unique_ptr<VipsImage, ImageUnref>;
using BufferPtr = std::unique_ptr<void, GFree>;
using HeifContextPtr = std::unique_ptr<heif_context, HeifContextFree>;
using HeifTrackPtr = std::unique_ptr<heif_track, HeifTrackRelease>;
using HeifImagePtr = std::unique_ptr<heif_image, HeifImageRelease>;
using HeifDecodingOptionsPtr =
    std::unique_ptr<heif_decoding_options, HeifDecodingOptionsFree>;
using EncoderPtr = std::unique_ptr<WebPAnimEncoder, EncoderDelete>;

constexpr std::size_t maximum_avif_frames = 1024;
constexpr std::size_t maximum_avif_decoded_pixels = 128'000'000;

[[nodiscard]] AnimatedConversionResult fail(
    AnimatedConversionError error) noexcept {
    vips_error_clear();
    return {.error = error, .body = {}};
}

[[nodiscard]] bool metadata_int(
    VipsImage *image,
    const char *name,
    int &value) noexcept {
    if (vips_image_get_typeof(image, name) == 0 || vips_image_get_int(image, name, &value) != 0) {
        vips_error_clear();
        return false;
    }
    return true;
}

} // namespace

AnimatedConversionResult convert_animated_image(
    std::span<const std::byte> body,
    ImageDimensions limits,
    EncodingQuality quality) {
    if (!initialize_vips()) {
        return fail(AnimatedConversionError::initialization);
    }
    if (body.empty()) {
        return fail(AnimatedConversionError::decode);
    }

    ImagePtr loaded(vips_image_new_from_buffer(
        body.data(), body.size(), "", "n", -1, nullptr));
    if (!loaded) {
        return fail(AnimatedConversionError::decode);
    }

    const int loaded_width = vips_image_get_width(loaded.get());
    const int loaded_height = vips_image_get_height(loaded.get());
    int page_count = 0;
    int page_height = 0;
    if (!metadata_int(loaded.get(), VIPS_META_N_PAGES, page_count) || !metadata_int(loaded.get(), VIPS_META_PAGE_HEIGHT, page_height) || page_count <= 0 || page_height <= 0 || loaded_height % page_height != 0 || loaded_height / page_height != page_count) {
        return fail(AnimatedConversionError::dimensions);
    }
    const auto dimensions = validate_dimensions(
        loaded_width, loaded_height, page_count, true);
    if (!dimensions.has_value() || dimensions->height != static_cast<std::uint32_t>(page_height)) {
        return fail(AnimatedConversionError::dimensions);
    }

    ImagePtr resized;
    VipsImage *current = loaded.get();
    if (const auto target = animated_resize_target(*dimensions, limits)) {
        VipsImage *thumbnail = nullptr;
        if (vips_thumbnail_image(loaded.get(), &thumbnail, target->width,
                                 "height", target->height,
                                 "crop", VIPS_INTERESTING_ALL,
                                 "size", VIPS_SIZE_DOWN,
                                 nullptr) != 0) {
            return fail(AnimatedConversionError::resize);
        }
        resized.reset(thumbnail);
        current = resized.get();
    }

    void *encoded_memory = nullptr;
    std::size_t encoded_size = 0;
    const int encode_result = vips_webpsave_buffer(current, &encoded_memory,
                                                   &encoded_size, "Q", encoding_quality_value(quality), "lossless", false,
                                                   nullptr);
    BufferPtr encoded(encoded_memory);
    if (encode_result != 0 || !encoded || encoded_size == 0) {
        return fail(AnimatedConversionError::encode);
    }

    const auto *bytes = static_cast<const std::byte *>(encoded.get());
    return {
        .error = AnimatedConversionError::none,
        .body = std::vector<std::byte>(bytes, bytes + encoded_size),
    };
}

AnimatedConversionResult convert_animated_avif(
    std::span<const std::byte> body,
    ImageDimensions limits,
    EncodingQuality quality) {
    if (body.empty()) {
        return fail(AnimatedConversionError::decode);
    }

    HeifContextPtr context(heif_context_alloc());
    if (!context || heif_context_read_from_memory_without_copy(context.get(), body.data(), body.size(), nullptr).code != heif_error_Ok || heif_context_has_sequence(context.get()) == 0) {
        return fail(AnimatedConversionError::decode);
    }

    HeifTrackPtr track(heif_context_get_track(context.get(), 0));
    if (!track || heif_track_get_track_handler_type(track.get()) != heif_track_type_image_sequence) {
        return fail(AnimatedConversionError::decode);
    }

    std::uint16_t width = 0;
    std::uint16_t height = 0;
    if (heif_track_get_image_resolution(track.get(), &width, &height).code != heif_error_Ok) {
        return fail(AnimatedConversionError::dimensions);
    }
    const auto dimensions = validate_dimensions(width, height, 1, false);
    if (!dimensions) {
        return fail(AnimatedConversionError::dimensions);
    }
    const std::size_t pixels_per_frame =
        static_cast<std::size_t>(width) * height;
    const std::uint32_t timescale = heif_track_get_timescale(track.get());
    if (timescale == 0) {
        return fail(AnimatedConversionError::decode);
    }

    const auto resized = animated_resize_target(*dimensions, limits);
    const ImageDimensions target = resized
                                       ? ImageDimensions{resized->width, resized->height}
                                       : *dimensions;
    if (target.width == 0 || target.height == 0 || target.width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) || target.height > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) {
        return fail(AnimatedConversionError::dimensions);
    }

    WebPAnimEncoderOptions encoder_options{};
    if (WebPAnimEncoderOptionsInit(&encoder_options) == 0) {
        return fail(AnimatedConversionError::encode);
    }
    EncoderPtr encoder(WebPAnimEncoderNew(
        static_cast<int>(target.width), static_cast<int>(target.height),
        &encoder_options));
    WebPConfig config{};
    if (!encoder || WebPConfigInit(&config) == 0) {
        return fail(AnimatedConversionError::encode);
    }
    config.lossless = 0;
    config.method = 0;
    config.quality = static_cast<float>(encoding_quality_value(quality));
    if (WebPValidateConfig(&config) == 0) {
        return fail(AnimatedConversionError::encode);
    }

    HeifDecodingOptionsPtr decoding_options(heif_decoding_options_alloc());
    if (!decoding_options) {
        return fail(AnimatedConversionError::decode);
    }
    // The edit list can request infinite repetition. Decode one timeline and
    // let the WebP animation container own playback repetition.
    decoding_options->ignore_sequence_editlist = 1;

    std::int32_t timestamp_ms = 0;
    std::size_t frame_count = 0;
    for (;;) {
        heif_image *raw_frame = nullptr;
        const heif_error error = heif_track_decode_next_image(track.get(),
                                                              &raw_frame, heif_colorspace_RGB,
                                                              heif_chroma_interleaved_RGBA, decoding_options.get());
        HeifImagePtr frame(raw_frame);
        if (error.code == heif_error_End_of_sequence) {
            break;
        }
        if (error.code != heif_error_Ok || !frame) {
            return fail(AnimatedConversionError::decode);
        }
        if (++frame_count > maximum_avif_frames || frame_count > maximum_avif_decoded_pixels / pixels_per_frame) {
            return fail(AnimatedConversionError::dimensions);
        }
        if (heif_image_get_width(frame.get(), heif_channel_interleaved) != width || heif_image_get_height(frame.get(), heif_channel_interleaved) != height) {
            return fail(AnimatedConversionError::dimensions);
        }

        std::size_t stride = 0;
        const std::uint8_t *pixels = heif_image_get_plane_readonly2(
            frame.get(), heif_channel_interleaved, &stride);
        const std::size_t packed_width = static_cast<std::size_t>(width) * 4;
        if (!pixels || stride < packed_width || stride > static_cast<std::size_t>(std::numeric_limits<int>::max()) || static_cast<std::size_t>(height) > std::numeric_limits<std::size_t>::max() / stride) {
            return fail(AnimatedConversionError::decode);
        }

        Picture picture;
        if (!picture.initialized) {
            return fail(AnimatedConversionError::encode);
        }
        picture.value.width = width;
        picture.value.height = height;
        picture.value.use_argb = 1;
        if (WebPPictureImportRGBA(&picture.value, pixels,
                                  static_cast<int>(stride)) == 0 ||
            (resized && WebPPictureRescale(&picture.value,
                                           static_cast<int>(target.width),
                                           static_cast<int>(target.height)) == 0) ||
            WebPAnimEncoderAdd(encoder.get(), &picture.value,
                               timestamp_ms, &config) == 0) {
            return fail(AnimatedConversionError::encode);
        }

        const std::uint64_t duration_ms = std::max<std::uint64_t>(1,
                                                                  static_cast<std::uint64_t>(heif_image_get_duration(frame.get())) * 1000U / timescale);
        if (duration_ms > static_cast<std::uint64_t>(
                              std::numeric_limits<std::int32_t>::max() - timestamp_ms)) {
            return fail(AnimatedConversionError::dimensions);
        }
        timestamp_ms += static_cast<std::int32_t>(duration_ms);
    }

    if (frame_count == 0) {
        return fail(AnimatedConversionError::decode);
    }
    if (WebPAnimEncoderAdd(encoder.get(), nullptr, timestamp_ms,
                           nullptr) == 0) {
        return fail(AnimatedConversionError::encode);
    }
    WebpData output;
    if (WebPAnimEncoderAssemble(encoder.get(), &output.value) == 0 || !output.value.bytes || output.value.size == 0) {
        return fail(AnimatedConversionError::encode);
    }
    const auto *begin = reinterpret_cast<const std::byte *>(output.value.bytes);
    return {
        .error = AnimatedConversionError::none,
        .body = std::vector<std::byte>(begin, begin + output.value.size),
    };
}

} // namespace mediaproxy::media
