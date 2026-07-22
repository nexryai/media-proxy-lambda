#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>
#include <mediaproxy/media/conversion.hpp>
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

using ImagePtr = std::unique_ptr<VipsImage, ImageUnref>;
using mediaproxy::media::ImageDimensions;
using mediaproxy::media::MimeType;
using mediaproxy::media::OutputFormat;
using mediaproxy::media::convert_media;
using mediaproxy::media::initialize_vips;

std::vector<std::byte> ReadFixture(const char* name)
{
    const std::string path = std::string{MEDIAPROXY_SOURCE_DIR}
        + "/tests/fixtures/media/apng/" + name;
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input) << path;
    const std::vector<char> bytes{
        std::istreambuf_iterator<char>(input), {}};
    const auto* begin = reinterpret_cast<const std::byte*>(bytes.data());
    return {begin, begin + bytes.size()};
}

ImagePtr LoadAll(const std::vector<std::byte>& body)
{
    return ImagePtr(vips_image_new_from_buffer(
        body.data(), body.size(), "", "n", -1, nullptr));
}

class MediaConversionTest : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(initialize_vips()) << vips_error_buffer();
    }
};

TEST_F(MediaConversionTest, NonPaletteApngIgnoresStaticPreferenceAndLimits)
{
    const auto result = convert_media(ReadFixture("over-none.png"),
        MimeType::image_png, true, OutputFormat::avif,
        ImageDimensions{1, 1});
    ASSERT_TRUE(result) << static_cast<int>(result.error) << ": "
                        << vips_error_buffer();
    EXPECT_EQ(result.encoded_format, OutputFormat::webp);
    const ImagePtr decoded = LoadAll(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 4);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 8);
    int pages = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES, &pages), 0);
    EXPECT_EQ(pages, 2);
}

TEST_F(MediaConversionTest, PaletteApngUsesStaticPreferredOutput)
{
    const auto result = convert_media(ReadFixture("palette-static.png"),
        MimeType::image_png, false, OutputFormat::avif,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error) << ": "
                        << vips_error_buffer();
    EXPECT_EQ(result.encoded_format, OutputFormat::avif);
    const ImagePtr decoded = LoadAll(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 2);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 2);
}

TEST_F(MediaConversionTest, RejectsUnsupportedMime)
{
    EXPECT_FALSE(convert_media({}, MimeType::text_plain_utf8, false,
        OutputFormat::webp, ImageDimensions{320, 320}));
}

} // namespace
