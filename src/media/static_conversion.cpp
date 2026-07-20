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

[[nodiscard]] std::uint16_t read_u16(
    std::span<const std::byte> body,
    std::size_t offset) noexcept
{
    return static_cast<std::uint16_t>(
        std::to_integer<std::uint8_t>(body[offset])
        | (std::to_integer<std::uint8_t>(body[offset + 1]) << 8U));
}

[[nodiscard]] std::uint32_t read_u32(
    std::span<const std::byte> body,
    std::size_t offset) noexcept
{
    const auto byte = [&body](std::size_t index) {
        return static_cast<std::uint32_t>(
            std::to_integer<std::uint8_t>(body[index]));
    };
    return byte(offset) | (byte(offset + 1) << 8U)
        | (byte(offset + 2) << 16U) | (byte(offset + 3) << 24U);
}

[[nodiscard]] ImagePtr load_ico_fallback(
    std::span<const std::byte> body)
{
    constexpr std::size_t header_size = 6;
    constexpr std::size_t entry_size = 16;
    if (body.size() < header_size || read_u16(body, 0) != 0
        || read_u16(body, 2) != 1) {
        return {};
    }
    const std::size_t count = read_u16(body, 4);
    if (count == 0
        || count > (body.size() - header_size) / entry_size) {
        return {};
    }

    for (std::size_t index = 0; index < count; ++index) {
        const std::size_t directory_offset = header_size + index * entry_size;
        const std::size_t payload_size = read_u32(body, directory_offset + 8);
        const std::size_t payload_offset = read_u32(body, directory_offset + 12);
        if (payload_size == 0 || payload_offset > body.size()
            || payload_size > body.size() - payload_offset) {
            continue;
        }
        const auto payload = body.subspan(payload_offset, payload_size);
        ImagePtr decoded(vips_image_new_from_buffer(
            payload.data(), payload.size(), "", nullptr));
        if (!decoded) {
            vips_error_clear();
            continue;
        }

        void* png_memory = nullptr;
        std::size_t png_size = 0;
        if (vips_pngsave_buffer(
                decoded.get(), &png_memory, &png_size, nullptr)
            != 0) {
            vips_error_clear();
            continue;
        }
        BufferPtr png(png_memory);
        ImagePtr reloaded(vips_image_new_from_buffer(
            png.get(), png_size, "", nullptr));
        if (reloaded) {
            // Buffer loaders are lazy and may retain the caller's bytes.
            // Materialize before the temporary encoded PNG is released.
            return ImagePtr(vips_image_copy_memory(reloaded.get()));
        }
        vips_error_clear();
    }
    return {};
}

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
    MimeType mime,
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
    if (!loaded
        && (mime == MimeType::image_ico
            || mime == MimeType::image_x_icon)) {
        vips_error_clear();
        loaded = load_ico_fallback(body);
    }
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
