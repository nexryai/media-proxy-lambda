#include <array>
#include <cstddef>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/media/classification.hpp>

namespace {

using mediaproxy::media::MediaPlan;
using mediaproxy::media::MimeType;
using mediaproxy::media::OutputFormat;
using mediaproxy::media::classify_media;
using mediaproxy::media::is_convertible_mime;

TEST(MediaClassification, AcceptsExactlySpecifiedMimeTypes)
{
    constexpr std::array accepted{
        MimeType::image_avif,
        MimeType::image_ico,
        MimeType::image_jpeg,
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
}

} // namespace
