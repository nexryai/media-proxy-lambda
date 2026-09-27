#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/media/classification.hpp>

namespace {

using mediaproxy::media::MediaPlan;
using mediaproxy::media::MimeType;
using mediaproxy::media::OutputFormat;
using mediaproxy::media::classify_media;
using mediaproxy::media::is_convertible_mime;

void WriteU32Be(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::uint32_t value)
{
    for (std::size_t index = 0; index < 4; ++index) {
        const unsigned int shift = static_cast<unsigned int>(24U - index * 8U);
        bytes[offset + index] = static_cast<std::byte>(value >> shift);
    }
}

void WriteAscii(
    std::vector<std::byte>& bytes,
    std::size_t offset,
    std::string_view value)
{
    for (std::size_t index = 0; index < value.size(); ++index) {
        bytes[offset + index] = static_cast<std::byte>(
            static_cast<unsigned char>(value[index]));
    }
}

std::vector<std::byte> MakeAvifFtyp(
    std::string_view major_brand,
    std::initializer_list<std::string_view> compatible_brands = {})
{
    const auto size = 16U + compatible_brands.size() * 4U;
    std::vector<std::byte> bytes(size, std::byte{0});
    WriteU32Be(bytes, 0, static_cast<std::uint32_t>(size));
    WriteAscii(bytes, 4, "ftyp");
    WriteAscii(bytes, 8, major_brand);
    std::size_t offset = 16;
    for (const std::string_view brand : compatible_brands) {
        WriteAscii(bytes, offset, brand);
        offset += 4;
    }
    return bytes;
}

TEST(MediaClassification, AcceptsExactlySpecifiedMimeTypes)
{
    constexpr std::array accepted{
        MimeType::image_avif,
        MimeType::image_ico,
        MimeType::image_jpeg,
        MimeType::image_jxl,
        MimeType::image_svg_xml,
        MimeType::image_png,
        MimeType::image_webp,
        MimeType::image_gif,
        MimeType::image_x_icon,
    };
    for (const MimeType mime : accepted) {
        EXPECT_TRUE(is_convertible_mime(mime));
    }

    constexpr std::array rejected{
        MimeType::image_bmp,
        MimeType::application_pdf,
        MimeType::application_postscript,
        MimeType::audio_mpeg,
        MimeType::application_ogg,
        MimeType::video_webm,
        MimeType::video_avi,
        MimeType::audio_wave,
        MimeType::application_zip,
        MimeType::application_x_gzip,
        MimeType::application_wasm,
        MimeType::text_html_utf8,
        MimeType::text_xml_utf8,
        MimeType::text_plain_utf8,
        MimeType::text_plain_utf16be,
        MimeType::text_plain_utf16le,
        MimeType::video_mp4,
        MimeType::font_ttf,
        MimeType::font_otf,
        MimeType::font_collection,
        MimeType::font_woff,
        MimeType::font_woff2,
        MimeType::application_eot,
        MimeType::application_octet_stream,
    };
    for (const MimeType mime : rejected) {
        EXPECT_FALSE(is_convertible_mime(mime));
        EXPECT_FALSE(classify_media(
            mime, {}, false, OutputFormat::webp).has_value());
    }
}

TEST(MediaClassification, GifIsAnimatedUnlessStaticIsForced)
{
    const auto animated = classify_media(
        MimeType::image_gif, {}, false, OutputFormat::avif);
    ASSERT_TRUE(animated.has_value());
    EXPECT_TRUE(animated->animated);
    EXPECT_EQ(animated->output, OutputFormat::webp);

    const auto static_image = classify_media(
        MimeType::image_gif, {}, true, OutputFormat::avif);
    ASSERT_TRUE(static_image.has_value());
    EXPECT_FALSE(static_image->animated);
    EXPECT_EQ(static_image->output, OutputFormat::avif);
}

TEST(MediaClassification, WebpRequiresAnimAtExactFixedOffset)
{
    std::vector<std::byte> body(34, std::byte{0});
    constexpr std::array tag{'A', 'N', 'I', 'M'};
    for (std::size_t index = 0; index < tag.size(); ++index) {
        body[0x1e + index] = static_cast<std::byte>(tag[index]);
    }

    auto plan = classify_media(
        MimeType::image_webp, body, false, OutputFormat::avif);
    ASSERT_TRUE(plan.has_value());
    EXPECT_TRUE(plan->animated);
    EXPECT_EQ(plan->output, OutputFormat::webp);

    body[0x1d] = static_cast<std::byte>('A');
    body[0x1e] = static_cast<std::byte>('X');
    plan = classify_media(
        MimeType::image_webp, body, false, OutputFormat::avif);
    ASSERT_TRUE(plan.has_value());
    EXPECT_FALSE(plan->animated);
    EXPECT_EQ(plan->output, OutputFormat::avif);

    body[0x1e] = static_cast<std::byte>('A');
    plan = classify_media(
        MimeType::image_webp, body, true, OutputFormat::avif);
    ASSERT_TRUE(plan.has_value());
    EXPECT_FALSE(plan->animated);
}

TEST(MediaClassification, AvifUsesAvisMajorOrCompatibleBrand)
{
    const auto major_brand = MakeAvifFtyp("avis");
    auto plan = classify_media(
        MimeType::image_avif, major_brand, false, OutputFormat::avif);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(*plan, (MediaPlan{true, OutputFormat::webp}));

    const auto compatible_brand = MakeAvifFtyp("avif", {"mif1", "avis"});
    plan = classify_media(
        MimeType::image_avif, compatible_brand, false, OutputFormat::avif);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(*plan, (MediaPlan{true, OutputFormat::webp}));

    plan = classify_media(
        MimeType::image_avif, major_brand, true, OutputFormat::avif);
    ASSERT_TRUE(plan.has_value());
    EXPECT_EQ(*plan, (MediaPlan{false, OutputFormat::avif}));
}

TEST(MediaClassification, AvifRequiresAvisInsideValidFtypBox)
{
    auto body = MakeAvifFtyp("avif", {"mif1", "avis"});
    ASSERT_TRUE(classify_media(
        MimeType::image_avif, body, false, OutputFormat::webp)->animated);

    WriteU32Be(body, 0, 16);
    EXPECT_FALSE(classify_media(
        MimeType::image_avif, body, false, OutputFormat::webp)->animated);

    body = MakeAvifFtyp("avif", {"avis"});
    WriteU32Be(body, 0, 19);
    EXPECT_FALSE(classify_media(
        MimeType::image_avif, body, false, OutputFormat::webp)->animated);

    body = MakeAvifFtyp("avis");
    WriteU32Be(body, 0, 17);
    EXPECT_FALSE(classify_media(
        MimeType::image_avif, body, false, OutputFormat::webp)->animated);

    body = MakeAvifFtyp("avis");
    WriteAscii(body, 4, "free");
    EXPECT_FALSE(classify_media(
        MimeType::image_avif, body, false, OutputFormat::webp)->animated);

    EXPECT_FALSE(classify_media(MimeType::image_avif,
        std::vector<std::byte>(15), false,
        OutputFormat::webp)->animated);
}

TEST(MediaClassification, OtherImagesRemainStaticAndUsePreference)
{
    const auto avif = classify_media(
        MimeType::image_png, {}, false, OutputFormat::avif);
    ASSERT_TRUE(avif.has_value());
    EXPECT_EQ(*avif, (MediaPlan{false, OutputFormat::avif}));

    const auto webp = classify_media(
        MimeType::image_avif, {}, false, OutputFormat::webp);
    ASSERT_TRUE(webp.has_value());
    EXPECT_EQ(*webp, (MediaPlan{false, OutputFormat::webp}));

    const auto jxl = classify_media(
        MimeType::image_jxl, {}, false, OutputFormat::avif);
    ASSERT_TRUE(jxl.has_value());
    EXPECT_EQ(*jxl, (MediaPlan{false, OutputFormat::avif}));

    const auto svg = classify_media(
        MimeType::image_svg_xml, {}, false, OutputFormat::webp);
    ASSERT_TRUE(svg.has_value());
    EXPECT_EQ(*svg, (MediaPlan{false, OutputFormat::webp}));
}

} // namespace
