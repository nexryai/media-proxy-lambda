#include <array>
#include <cstddef>
#include <cstdint>
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
#include <mediaproxy/media/mime.hpp>
#include <mediaproxy/media/vips_runtime.hpp>
#include <openssl/sha.h>
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
using mediaproxy::media::sniff_mime;

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
             std::pair{"animated/elephant.gif", MimeType::image_gif},
             std::pair{"animated/800x450_2.avif", MimeType::image_avif}}) {
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

TEST_F(MediaConversionTest, ConvertsAvisSequenceWithFrameTiming)
{
    const auto input = ReadMediaFixture("animated/800x450_2.avif");
    ASSERT_FALSE(input.empty());
    const MimeType mime = sniff_mime(input);
    ASSERT_EQ(mime, MimeType::image_avif);

    const auto animated = convert_media(input, mime, false,
        OutputFormat::avif, ImageDimensions{320, 180});
    ASSERT_TRUE(animated) << static_cast<int>(animated.error);
    EXPECT_EQ(animated.encoded_format, OutputFormat::webp);
    std::array<std::uint8_t, SHA256_DIGEST_LENGTH> digest{};
    ASSERT_EQ(::SHA256(reinterpret_cast<const std::uint8_t*>(
                    animated.body.data()), animated.body.size(),
                  digest.data()), digest.data());
    constexpr char hex[] = "0123456789abcdef";
    std::string encoded_hash;
    for (const std::uint8_t byte : digest) {
        encoded_hash.push_back(hex[byte >> 4U]);
        encoded_hash.push_back(hex[byte & 0x0fU]);
    }
    EXPECT_EQ(encoded_hash,
        "9b6c7b5d9ead9ad86ff9aa4e37d5ef560855cf159ea200aea2b77551a515229b");
    const ImagePtr decoded = LoadAll(animated.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    int page_count = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES,
                  &page_count), 0);
    EXPECT_EQ(page_count, 99);
    int* delays = nullptr;
    int delay_count = 0;
    ASSERT_EQ(vips_image_get_array_int(decoded.get(), "delay", &delays,
                  &delay_count), 0);
    ASSERT_EQ(delay_count, 99);
    std::int32_t total_duration = 0;
    for (int index = 0; index < delay_count; ++index) {
        total_duration += delays[index];
    }
    EXPECT_EQ(delays[0], 40);
    EXPECT_EQ(delays[delay_count - 1], 40);
    EXPECT_EQ(total_duration, 4000);

    const auto static_image = convert_media(input, mime, true,
        OutputFormat::avif, ImageDimensions{320, 180});
    ASSERT_TRUE(static_image) << static_cast<int>(static_image.error);
    EXPECT_EQ(static_image.encoded_format, OutputFormat::avif);
    const ImagePtr static_decoded = LoadAll(static_image.body);
    ASSERT_NE(static_decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(static_decoded.get()), 320);
    EXPECT_EQ(vips_image_get_height(static_decoded.get()), 180);
}

TEST_F(MediaConversionTest, RejectsMalformedAvisSequence)
{
    std::array<std::byte, 16> body{};
    body[3] = std::byte{16};
    constexpr std::string_view signature = "ftypavis";
    for (std::size_t index = 0; index < signature.size(); ++index) {
        body[4 + index] = static_cast<std::byte>(signature[index]);
    }
    ASSERT_EQ(sniff_mime(body), MimeType::image_avif);
    const auto result = convert_media(body, MimeType::image_avif, false,
        OutputFormat::avif, ImageDimensions{320, 320});
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, mediaproxy::media::MediaConversionError::convert);
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
