#include <gtest/gtest.h>

#include <glib.h>
#include <vips/vips.h>

#include <cstddef>
#include <memory>

namespace {

struct VipsImageUnref {
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

using ImagePtr = std::unique_ptr<VipsImage, VipsImageUnref>;
using BufferPtr = std::unique_ptr<void, GFree>;

TEST(LibvipsBuild, ProvidesAvifWithoutOptionalForeignLoaders)
{
    ASSERT_EQ(vips_init("mediaproxy-smoke"), 0) << vips_error_buffer();
    vips_concurrency_set(1);
    vips_cache_set_max(0);

    EXPECT_STREQ(vips_version_string(), "8.18.2");
    EXPECT_NE(vips_type_find("VipsOperation", "heifload"), 0U);
    EXPECT_NE(vips_type_find("VipsOperation", "heifsave"), 0U);
    EXPECT_NE(vips_type_find("VipsOperation", "jpegload"), 0U);
    EXPECT_NE(vips_type_find("VipsOperation", "pngload"), 0U);
    EXPECT_NE(vips_type_find("VipsOperation", "webpload"), 0U);
    EXPECT_NE(vips_type_find("VipsOperation", "gifload"), 0U);

    EXPECT_EQ(vips_type_find("VipsOperation", "tiffload"), 0U);
    EXPECT_EQ(vips_type_find("VipsOperation", "magickload"), 0U);
    EXPECT_EQ(vips_type_find("VipsOperation", "jp2kload"), 0U);
    EXPECT_EQ(vips_type_find("VipsOperation", "jxlload"), 0U);
    EXPECT_EQ(vips_type_find("VipsOperation", "svgload"), 0U);
    EXPECT_EQ(vips_type_find("VipsOperation", "svgload_buffer"), 0U);
    EXPECT_EQ(vips_type_find("VipsOperation", "text"), 0U);

    VipsImage* raw_image = nullptr;
    ASSERT_EQ(vips_black(&raw_image, 2, 3, nullptr), 0)
        << vips_error_buffer();
    ImagePtr image(raw_image);

    void* raw_buffer = nullptr;
    std::size_t encoded_size = 0;
    ASSERT_EQ(vips_heifsave_buffer(image.get(), &raw_buffer, &encoded_size,
                  "compression", VIPS_FOREIGN_HEIF_COMPRESSION_AV1, nullptr),
        0)
        << vips_error_buffer();
    BufferPtr encoded(raw_buffer);
    ASSERT_NE(encoded.get(), nullptr);
    ASSERT_GT(encoded_size, 0U);

    VipsImage* raw_decoded = nullptr;
    ASSERT_EQ(vips_heifload_buffer(
                  encoded.get(), encoded_size, &raw_decoded, nullptr),
        0)
        << vips_error_buffer();
    ImagePtr decoded(raw_decoded);
    EXPECT_EQ(vips_image_get_width(decoded.get()), 2);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 3);

    decoded.reset();
    image.reset();
    encoded.reset();
    vips_shutdown();
}

} // namespace
