#include <mediaproxy/media/animated_conversion.hpp>

#include <cstddef>
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

[[nodiscard]] AnimatedConversionResult fail(
    AnimatedConversionError error) noexcept
{
    vips_error_clear();
    return {.error = error, .body = {}};
}

[[nodiscard]] bool metadata_int(
    VipsImage* image,
    const char* name,
    int& value) noexcept
{
    if (vips_image_get_typeof(image, name) == 0
        || vips_image_get_int(image, name, &value) != 0) {
        vips_error_clear();
        return false;
    }
    return true;
}

} // namespace

AnimatedConversionResult convert_animated_image(
    std::span<const std::byte> body,
    ImageDimensions limits)
{
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
    if (!metadata_int(loaded.get(), VIPS_META_N_PAGES, page_count)
        || !metadata_int(loaded.get(), VIPS_META_PAGE_HEIGHT, page_height)
        || page_count <= 0 || page_height <= 0
        || loaded_height % page_height != 0
        || loaded_height / page_height != page_count) {
        return fail(AnimatedConversionError::dimensions);
    }
    const auto dimensions = validate_dimensions(
        loaded_width, loaded_height, page_count, true);
    if (!dimensions.has_value()
        || dimensions->height != static_cast<std::uint32_t>(page_height)) {
        return fail(AnimatedConversionError::dimensions);
    }

    ImagePtr resized;
    VipsImage* current = loaded.get();
    if (const auto target = animated_resize_target(*dimensions, limits)) {
        VipsImage* thumbnail = nullptr;
        if (vips_thumbnail_image(loaded.get(), &thumbnail, target->width,
                "height", target->height,
                "crop", VIPS_INTERESTING_ALL,
                "size", VIPS_SIZE_DOWN,
                nullptr)
            != 0) {
            return fail(AnimatedConversionError::resize);
        }
        resized.reset(thumbnail);
        current = resized.get();
    }

    void* encoded_memory = nullptr;
    std::size_t encoded_size = 0;
    const int encode_result = vips_webpsave_buffer(current, &encoded_memory,
        &encoded_size, "Q", 70, "lossless", false, nullptr);
    BufferPtr encoded(encoded_memory);
    if (encode_result != 0 || !encoded || encoded_size == 0) {
        return fail(AnimatedConversionError::encode);
    }

    const auto* bytes = static_cast<const std::byte*>(encoded.get());
    return {
        .error = AnimatedConversionError::none,
        .body = std::vector<std::byte>(bytes, bytes + encoded_size),
    };
}

} // namespace mediaproxy::media
