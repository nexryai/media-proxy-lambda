#include <mediaproxy/media/conversion.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <utility>

#include <glib.h>
#include <mediaproxy/media/animated_conversion.hpp>
#include <mediaproxy/media/apng.hpp>
#include <mediaproxy/media/apng_conversion.hpp>
#include <mediaproxy/media/static_conversion.hpp>
#include <mediaproxy/media/vips_runtime.hpp>
#include <vips/vips.h>

namespace mediaproxy::media {
namespace {

struct ImageUnref {
    void operator()(VipsImage* image) const noexcept
    {
        if (image != nullptr) {
            g_object_unref(image);
        }
    }
};

using ImagePtr = std::unique_ptr<VipsImage, ImageUnref>;

[[nodiscard]] MediaConversionResult fail(MediaConversionError error)
{
    vips_error_clear();
    return {.error = error, .encoded_format = OutputFormat::webp, .body = {}};
}

[[nodiscard]] MediaConversionResult convert_nonpalette_apng(
    std::span<const std::byte> body)
{
    if (!initialize_vips()) {
        return fail(MediaConversionError::decode);
    }
    ImagePtr loaded(vips_image_new_from_buffer(
        body.data(), body.size(), "", nullptr));
    if (!loaded) {
        return fail(MediaConversionError::decode);
    }
    const int width = vips_image_get_width(loaded.get());
    const int height = vips_image_get_height(loaded.get());
    if (width <= 0 || height <= 0) {
        return fail(MediaConversionError::decode);
    }
    auto converted = convert_apng_to_webp(body,
        static_cast<std::uint32_t>(width),
        static_cast<std::uint32_t>(height));
    if (!converted) {
        return fail(MediaConversionError::convert);
    }
    return {
        .error = MediaConversionError::none,
        .encoded_format = OutputFormat::webp,
        .body = std::move(converted.body),
    };
}

} // namespace

MediaConversionResult convert_media(
    std::span<const std::byte> body,
    MimeType mime,
    bool force_static,
    OutputFormat preferred_output,
    ImageDimensions limits)
{
    if (mime == MimeType::image_png) {
        const auto apng = classify_apng(body);
        if (apng == ApngClassification::animated) {
            return convert_nonpalette_apng(body);
        }
        // Palette APNG intentionally continues through the static path.
    }

    const auto plan = classify_media(
        mime, body, force_static, preferred_output);
    if (!plan.has_value()) {
        return fail(MediaConversionError::unsupported);
    }
    if (plan->animated) {
        auto converted = convert_animated_image(body, limits);
        if (!converted) {
            return fail(MediaConversionError::convert);
        }
        return {
            .error = MediaConversionError::none,
            .encoded_format = OutputFormat::webp,
            .body = std::move(converted.body),
        };
    }

    auto converted = convert_static_image(body, mime, plan->output, limits);
    if (!converted) {
        return fail(MediaConversionError::convert);
    }
    return {
        .error = MediaConversionError::none,
        .encoded_format = plan->output,
        .body = std::move(converted.body),
    };
}

} // namespace mediaproxy::media
