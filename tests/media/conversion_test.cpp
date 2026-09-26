#include <array>
#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>
#include <mediaproxy/media/apng_conversion.hpp>
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
using mediaproxy::media::EncodingQuality;
using mediaproxy::media::MimeType;
using mediaproxy::media::OutputFormat;
using mediaproxy::media::convert_media;
using mediaproxy::media::initialize_vips;
using mediaproxy::media::encoding_quality_value;

std::vector<std::byte> ReadMediaFixture(std::string_view path)
{
    const std::string full_path = std::string{MEDIAPROXY_SOURCE_DIR}
        + "/tests/fixtures/media/" + std::string{path};
    std::ifstream input(full_path, std::ios::binary);
    EXPECT_TRUE(input) << full_path;
    const std::vector<char> bytes{
        std::istreambuf_iterator<char>(input), {}};
    const auto* begin = reinterpret_cast<const std::byte*>(bytes.data());
    return {begin, begin + bytes.size()};
}

std::vector<std::byte> ReadFixture(const char* name)
{
    return ReadMediaFixture(std::string{"apng/"} + name);
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

TEST_F(MediaConversionTest, UsesSharedVipsQualityForStaticWebpAndAvif)
{
    EXPECT_EQ(encoding_quality_value(EncodingQuality::standard), 65);
    EXPECT_EQ(encoding_quality_value(EncodingQuality::url_only), 70);
    const auto input = ReadMediaFixture("resize/1500x843.jpg");
    ASSERT_FALSE(input.empty());
    for (const auto output : {OutputFormat::webp, OutputFormat::avif}) {
        SCOPED_TRACE(static_cast<int>(output));
        const auto standard = convert_media(input, MimeType::image_jpeg, false,
            output, ImageDimensions{320, 320}, EncodingQuality::standard);
        const auto url_only = convert_media(input, MimeType::image_jpeg, false,
            output, ImageDimensions{320, 320}, EncodingQuality::url_only);
        ASSERT_TRUE(standard) << static_cast<int>(standard.error);
        ASSERT_TRUE(url_only) << static_cast<int>(url_only.error);
        EXPECT_NE(standard.body, url_only.body);
    }
}

TEST_F(MediaConversionTest, AppliesSharedQualityToAnimatedAndApng)
{
    for (const auto& [path, mime] : {
             std::pair{"animated/animated-webp-supported.webp",
                 MimeType::image_webp},
             std::pair{"animated/elephant.gif", MimeType::image_gif}}) {
        SCOPED_TRACE(path);
        const auto animation = ReadMediaFixture(path);
        ASSERT_FALSE(animation.empty());
        const auto standard = convert_media(animation, mime,
            false, OutputFormat::webp, ImageDimensions{320, 320},
            EncodingQuality::standard);
        const auto url_only = convert_media(animation, mime,
            false, OutputFormat::webp, ImageDimensions{320, 320},
            EncodingQuality::url_only);
        ASSERT_TRUE(standard) << static_cast<int>(standard.error);
        ASSERT_TRUE(url_only) << static_cast<int>(url_only.error);
        EXPECT_NE(standard.body, url_only.body);
    }

    const auto apng = ReadFixture("issue-2-color-timing.png");
    const auto apng_standard = convert_media(apng, MimeType::image_png, false,
        OutputFormat::webp, ImageDimensions{320, 320},
        EncodingQuality::standard);
    const auto apng_url_only = convert_media(apng, MimeType::image_png, false,
        OutputFormat::webp, ImageDimensions{320, 320},
        EncodingQuality::url_only);
    ASSERT_TRUE(apng_standard);
    ASSERT_TRUE(apng_url_only);
    const auto direct_standard = mediaproxy::media::convert_apng_to_webp(
        apng, 4, 4, EncodingQuality::standard);
    const auto direct_url_only = mediaproxy::media::convert_apng_to_webp(
        apng, 4, 4, EncodingQuality::url_only);
    ASSERT_TRUE(direct_standard);
    ASSERT_TRUE(direct_url_only);
    EXPECT_EQ(apng_standard.body, direct_standard.body);
    EXPECT_EQ(apng_url_only.body, direct_url_only.body);
}

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
    EXPECT_EQ(vips_image_get_height(decoded.get()), 12);
    int pages = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES, &pages), 0);
    EXPECT_EQ(pages, 3);
}

TEST_F(MediaConversionTest, PaletteApngRetainsAnimationDespiteStaticPreference)
{
    const auto result = convert_media(ReadFixture("palette-alpha.png"),
        MimeType::image_png, true, OutputFormat::avif,
        ImageDimensions{1, 1});
    ASSERT_TRUE(result) << static_cast<int>(result.error) << ": "
                        << vips_error_buffer();
    EXPECT_EQ(result.encoded_format, OutputFormat::webp);
    const ImagePtr decoded = LoadAll(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 2);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 4);
    int pages = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES, &pages), 0);
    EXPECT_EQ(pages, 2);
}

TEST_F(MediaConversionTest, PaletteFallbackImageIsNotAnAnimationFrame)
{
    const auto result = convert_media(ReadFixture("palette-fallback.png"),
        MimeType::image_png, false, OutputFormat::webp,
        ImageDimensions{3200, 3200});
    ASSERT_TRUE(result) << static_cast<int>(result.error) << ": "
                        << vips_error_buffer();
    EXPECT_EQ(result.encoded_format, OutputFormat::webp);
    const ImagePtr decoded = LoadAll(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    int pages = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES, &pages), 0);
    EXPECT_EQ(pages, 2);
}

TEST_F(MediaConversionTest, PaletteChunksAfterFrameControlAndOpaqueEntriesConvert)
{
    struct Case {
        const char* file;
        int pages;
    };
    constexpr std::array cases{
        Case{"palette-after-first-control.png", 3},
        Case{"palette-opaque.png", 2},
    };
    for (const auto& test_case : cases) {
        SCOPED_TRACE(test_case.file);
        const auto result = convert_media(ReadFixture(test_case.file),
            MimeType::image_png, false, OutputFormat::avif,
            ImageDimensions{3200, 3200});
        ASSERT_TRUE(result) << static_cast<int>(result.error) << ": "
                            << vips_error_buffer();
        EXPECT_EQ(result.encoded_format, OutputFormat::webp);
        const ImagePtr decoded = LoadAll(result.body);
        ASSERT_NE(decoded, nullptr) << vips_error_buffer();
        int pages = 0;
        ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES,
                      &pages),
            0);
        EXPECT_EQ(pages, test_case.pages);
    }
}

TEST_F(MediaConversionTest, ApngWithAncillaryChunksRemainsAnimated)
{
    const auto result = convert_media(
        ReadFixture("animation-control-other-offset.png"),
        MimeType::image_png, false, OutputFormat::avif,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error) << ": "
                        << vips_error_buffer();
    EXPECT_EQ(result.encoded_format, OutputFormat::webp);
    const ImagePtr decoded = LoadAll(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 4);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 8);
}

TEST_F(MediaConversionTest, RejectsUnsupportedMime)
{
    EXPECT_FALSE(convert_media({}, MimeType::text_plain_utf8, false,
        OutputFormat::webp, ImageDimensions{320, 320}));
}

} // namespace
