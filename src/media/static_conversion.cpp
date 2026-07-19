#include <mediaproxy/media/static_conversion.hpp>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <glib.h>
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

struct GFree {
    void operator()(void* memory) const noexcept
    {
        g_free(memory);
    }
};

using ImagePtr = std::unique_ptr<VipsImage, ImageUnref>;
using BufferPtr = std::unique_ptr<void, GFree>;

[[nodiscard]] StaticConversionResult fail(
    StaticConversionError error) noexcept
{
    vips_error_clear();
    return {.error = error, .body = {}};
}

[[nodiscard]] int image_metadata_int(
    VipsImage* image,
    const char* name,
    int fallback) noexcept
{
    int value = fallback;
    if (vips_image_get_typeof(image, name) != 0
        && vips_image_get_int(image, name, &value) != 0) {
        vips_error_clear();
        return fallback;
    }
    return value;
}

} // namespace

StaticConversionResult convert_static_image(
    std::span<const std::byte> body,
    OutputFormat output,
    ImageDimensions limits)
{
    if (!initialize_vips()) {
        return fail(StaticConversionError::initialization);
    }
    if (body.empty()) {
        return fail(StaticConversionError::decode);
    }

    ImagePtr loaded(vips_image_new_from_buffer(
        body.data(), body.size(), "", "n", -1, nullptr));
    if (!loaded) {
        return fail(StaticConversionError::decode);
    }

    const int loaded_width = vips_image_get_width(loaded.get());
    const int loaded_height = vips_image_get_height(loaded.get());
    const int page_count =
        image_metadata_int(loaded.get(), VIPS_META_N_PAGES, 1);
    const int page_height = image_metadata_int(
        loaded.get(), VIPS_META_PAGE_HEIGHT, loaded_height);
    if (page_count <= 0 || page_height <= 0
        || loaded_height % page_height != 0
        || loaded_height / page_height != page_count) {
        return fail(StaticConversionError::dimensions);
    }
    const auto dimensions =
        validate_dimensions(loaded_width, page_height, 1, false);
    if (!dimensions.has_value()) {
        return fail(StaticConversionError::dimensions);
    }

    ImagePtr first_page;
    VipsImage* current = loaded.get();
    if (loaded_height != page_height) {
        VipsImage* cropped = nullptr;
        if (vips_crop(loaded.get(), &cropped, 0, 0, loaded_width,
                page_height, nullptr)
            != 0) {
            return fail(StaticConversionError::decode);
        }
        first_page.reset(cropped);
        current = first_page.get();
    }

    ImagePtr resized;
    if (const auto scale = static_resize_scale(*dimensions, limits)) {
        VipsImage* resized_image = nullptr;
        // The pinned libvips has no VIPS_KERNEL_AUTO enum. Leaving the
        // optional kernel unset is its API for automatic/default selection.
        if (vips_resize(current, &resized_image, *scale, nullptr) != 0) {
            return fail(StaticConversionError::resize);
        }
        resized.reset(resized_image);
        current = resized.get();
    }

    void* encoded_memory = nullptr;
    std::size_t encoded_size = 0;
    const int encode_result = output == OutputFormat::avif
        ? vips_heifsave_buffer(current, &encoded_memory, &encoded_size,
              "Q", 65,
              "effort", 1,
              "lossless", false,
              "compression", VIPS_FOREIGN_HEIF_COMPRESSION_AV1,
              nullptr)
        : vips_webpsave_buffer(current, &encoded_memory, &encoded_size,
              "Q", 70,
              "lossless", false,
              nullptr);
    BufferPtr encoded(encoded_memory);
    if (encode_result != 0 || !encoded || encoded_size == 0) {
        return fail(StaticConversionError::encode);
    }

    const auto* bytes = static_cast<const std::byte*>(encoded.get());
    return {
        .error = StaticConversionError::none,
        .body = std::vector<std::byte>(bytes, bytes + encoded_size),
    };
}

} // namespace mediaproxy::media
