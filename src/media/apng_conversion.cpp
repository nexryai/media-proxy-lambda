#include <mediaproxy/media/apng_conversion.hpp>

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <vector>

#include <mediaproxy/media/apng_compositor.hpp>
#include <mediaproxy/media/apng_decoder.hpp>
#include <webp/encode.h>
#include <webp/mux.h>

namespace mediaproxy::media {
namespace {

struct EncoderDelete {
    void operator()(WebPAnimEncoder* encoder) const noexcept
    {
        WebPAnimEncoderDelete(encoder);
    }
};

struct Picture {
    WebPPicture value{};
    bool initialized = false;

    Picture()
        : initialized(WebPPictureInit(&value) != 0)
    {
    }

    ~Picture()
    {
        if (initialized) {
            WebPPictureFree(&value);
        }
    }

    Picture(const Picture&) = delete;
    Picture& operator=(const Picture&) = delete;
};

struct WebpData {
    WebPData value{};

    WebpData() { WebPDataInit(&value); }
    ~WebpData() { WebPDataClear(&value); }

    WebpData(const WebpData&) = delete;
    WebpData& operator=(const WebpData&) = delete;
};

using EncoderPtr = std::unique_ptr<WebPAnimEncoder, EncoderDelete>;

[[nodiscard]] ApngConversionResult fail(ApngConversionError error)
{
    return {.error = error, .body = {}};
}

[[nodiscard]] bool canvas_size(
    std::uint32_t width,
    std::uint32_t height,
    std::size_t& output) noexcept
{
    if (width == 0 || height == 0
        || width > std::numeric_limits<std::size_t>::max() / height) {
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(width) * height;
    if (pixels > std::numeric_limits<std::size_t>::max() / 4) {
        return false;
    }
    output = pixels * 4;
    return true;
}

} // namespace

ApngConversionResult convert_apng_to_webp(
    std::span<const std::byte> body,
    std::uint32_t target_width,
    std::uint32_t target_height)
{
    if (target_width == 0 || target_height == 0
        || target_width > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())
        || target_height > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())) {
        return fail(ApngConversionError::dimensions);
    }

    auto decoded = decode_apng_frames(body);
    if (!decoded || decoded.frames.empty()) {
        return fail(ApngConversionError::decode);
    }
    if (decoded.canvas_width > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max() / 4)
        || decoded.canvas_height > static_cast<std::uint32_t>(
            std::numeric_limits<int>::max())) {
        return fail(ApngConversionError::dimensions);
    }
    std::size_t expected_canvas_size = 0;
    if (!canvas_size(
            decoded.canvas_width, decoded.canvas_height, expected_canvas_size)
        || decoded.frames.front().rgba.size() != expected_canvas_size) {
        return fail(ApngConversionError::base_frame);
    }
    std::vector<std::byte> canvas = std::move(decoded.frames.front().rgba);

    WebPAnimEncoderOptions options{};
    if (WebPAnimEncoderOptionsInit(&options) == 0) {
        return fail(ApngConversionError::encoder);
    }
    EncoderPtr encoder(WebPAnimEncoderNew(
        static_cast<int>(target_width), static_cast<int>(target_height),
        &options));
    if (!encoder) {
        return fail(ApngConversionError::encoder);
    }

    for (std::size_t index = 1; index < decoded.frames.size(); ++index) {
        const auto& frame = decoded.frames[index];
        auto composed = compose_apng_frame(canvas, decoded.canvas_width,
            decoded.canvas_height, frame.control, frame.rgba);
        if (!composed) {
            return fail(ApngConversionError::composition);
        }

        Picture picture;
        if (!picture.initialized) {
            return fail(ApngConversionError::picture);
        }
        picture.value.width = static_cast<int>(decoded.canvas_width);
        picture.value.height = static_cast<int>(decoded.canvas_height);
        if (WebPPictureImportRGBA(&picture.value,
                reinterpret_cast<const std::uint8_t*>(
                    composed.displayed_rgba.data()),
                static_cast<int>(decoded.canvas_width * 4U))
                == 0
            || WebPPictureRescale(&picture.value,
                   static_cast<int>(target_width),
                   static_cast<int>(target_height))
                == 0) {
            return fail(ApngConversionError::picture);
        }
        const auto timestamp = apng_frame_timestamp_ms(
            static_cast<std::uint32_t>(index),
            frame.control.delay_numerator,
            frame.control.delay_denominator);
        if (WebPAnimEncoderAdd(
                encoder.get(), &picture.value, timestamp, nullptr)
            == 0) {
            return fail(ApngConversionError::encoder);
        }
    }

    WebpData output;
    if (WebPAnimEncoderAssemble(encoder.get(), &output.value) == 0
        || output.value.bytes == nullptr || output.value.size == 0) {
        return fail(ApngConversionError::encoder);
    }
    const auto* begin = reinterpret_cast<const std::byte*>(output.value.bytes);
    return {
        .error = ApngConversionError::none,
        .body = std::vector<std::byte>(begin, begin + output.value.size),
    };
}

} // namespace mediaproxy::media
