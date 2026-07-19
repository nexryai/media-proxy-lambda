#include <cstddef>
#include <memory>
#include <span>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>
#include <mediaproxy/media/animated_conversion.hpp>
#include <mediaproxy/media/vips_runtime.hpp>
#include <vips/vips.h>

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
using mediaproxy::media::ImageDimensions;
using mediaproxy::media::convert_animated_image;
using mediaproxy::media::initialize_vips;

std::vector<std::byte> MakeAnimation(int width, int page_height)
{
    VipsImage* raw_first = nullptr;
    VipsImage* raw_dark = nullptr;
    EXPECT_EQ(vips_black(
                  &raw_first, width, page_height, "bands", 3, nullptr),
        0);
    EXPECT_EQ(vips_black(&raw_dark, width, page_height, "bands", 3, nullptr),
        0);
    ImagePtr first(raw_first);
    ImagePtr dark(raw_dark);

    VipsImage* raw_second = nullptr;
    const double scale[] = {1.0};
    const double offset[] = {255.0};
    EXPECT_EQ(vips_linear(dark.get(), &raw_second, scale, offset, 1, nullptr),
        0);
    ImagePtr second(raw_second);

    VipsImage* raw_joined = nullptr;
    EXPECT_EQ(vips_join(first.get(), second.get(), &raw_joined,
                  VIPS_DIRECTION_VERTICAL, nullptr),
        0);
    ImagePtr joined(raw_joined);
    vips_image_set_int(joined.get(), VIPS_META_PAGE_HEIGHT, page_height);
    vips_image_set_int(joined.get(), VIPS_META_N_PAGES, 2);
    const int delays[] = {40, 90};
    vips_image_set_array_int(joined.get(), "delay", delays, 2);

    void* raw_buffer = nullptr;
    std::size_t size = 0;
    EXPECT_EQ(vips_webpsave_buffer(
                  joined.get(), &raw_buffer, &size, "Q", 70, nullptr),
        0)
        << vips_error_buffer();
    BufferPtr buffer(raw_buffer);
    const auto* bytes = static_cast<const std::byte*>(buffer.get());
    return {bytes, bytes + size};
}

ImagePtr LoadAll(std::span<const std::byte> body)
{
    return ImagePtr(vips_image_new_from_buffer(
        body.data(), body.size(), "", "n", -1, nullptr));
}

class AnimatedConversionTest : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(initialize_vips()) << vips_error_buffer();
    }
};

TEST_F(AnimatedConversionTest, PreservesPagesAndResizesFrames)
{
    const auto input = MakeAnimation(100, 80);
    const auto result =
        convert_animated_image(input, ImageDimensions{50, 40});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    const ImagePtr decoded = LoadAll(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 50);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 80);
    int pages = 0;
    int page_height = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES, &pages), 0);
    ASSERT_EQ(vips_image_get_int(
                  decoded.get(), VIPS_META_PAGE_HEIGHT, &page_height),
        0);
    EXPECT_EQ(pages, 2);
    EXPECT_EQ(page_height, 40);
    int* delays = nullptr;
    int delay_count = 0;
    ASSERT_EQ(vips_image_get_array_int(
                  decoded.get(), "delay", &delays, &delay_count),
        0);
    ASSERT_EQ(delay_count, 2);
    EXPECT_EQ(delays[0], 40);
    EXPECT_EQ(delays[1], 90);
}

TEST_F(AnimatedConversionTest, RejectsStaticAndMalformedInputs)
{
    const auto animation = MakeAnimation(8, 6);
    const ImagePtr all_pages = LoadAll(animation);
    ASSERT_NE(all_pages, nullptr);
    VipsImage* raw_first = nullptr;
    ASSERT_EQ(vips_crop(all_pages.get(), &raw_first, 0, 0, 8, 6, nullptr), 0);
    ImagePtr first(raw_first);
    void* raw_static = nullptr;
    std::size_t static_size = 0;
    ASSERT_EQ(vips_webpsave_buffer(
                  first.get(), &raw_static, &static_size, nullptr),
        0);
    BufferPtr static_buffer(raw_static);
    const auto* bytes = static_cast<const std::byte*>(static_buffer.get());
    EXPECT_FALSE(convert_animated_image(
        std::span(bytes, static_size), ImageDimensions{320, 320}));
    EXPECT_FALSE(convert_animated_image(
        {}, ImageDimensions{320, 320}));
}

} // namespace
