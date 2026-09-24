#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>
#include <mediaproxy/media/apng_conversion.hpp>
#include <mediaproxy/media/conversion.hpp>
#include <mediaproxy/media/vips_runtime.hpp>
#include <vips/vips.h>
#include <webp/demux.h>

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
using mediaproxy::media::convert_apng_to_webp;
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

class ApngConversionTest : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(initialize_vips()) << vips_error_buffer();
    }
};

TEST_F(ApngConversionTest, EmitsEveryCallbackAndUsesTargetDimensions)
{
    const auto result =
        convert_apng_to_webp(ReadFixture("over-none.png"), 8, 6);
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    ImagePtr decoded(vips_image_new_from_buffer(result.body.data(),
        result.body.size(), "", "n", -1, nullptr));
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 8);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 18);
    int pages = 0;
    int page_height = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES, &pages), 0);
    ASSERT_EQ(vips_image_get_int(
                  decoded.get(), VIPS_META_PAGE_HEIGHT, &page_height),
        0);
    EXPECT_EQ(pages, 3);
    EXPECT_EQ(page_height, 6);
}

TEST_F(ApngConversionTest, PreservesIssueOneFirstFrame)
{
    const auto result = convert_apng_to_webp(
        ReadFixture("issue-1-first-frame.png"), 4, 4);
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    ImagePtr decoded(vips_image_new_from_buffer(result.body.data(),
        result.body.size(), "", "n", -1, nullptr));
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    int pages = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES, &pages), 0);
    EXPECT_EQ(pages, 5);

    const WebPData webp{
        .bytes = reinterpret_cast<const std::uint8_t*>(result.body.data()),
        .size = result.body.size(),
    };
    using DecoderPtr = std::unique_ptr<WebPAnimDecoder,
        decltype(&WebPAnimDecoderDelete)>;
    DecoderPtr decoder(WebPAnimDecoderNew(&webp, nullptr),
        &WebPAnimDecoderDelete);
    ASSERT_NE(decoder, nullptr);
    std::uint8_t* pixels = nullptr;
    int timestamp = -1;
    ASSERT_EQ(WebPAnimDecoderGetNext(decoder.get(), &pixels, &timestamp), 1);
    ASSERT_NE(pixels, nullptr);
    // The decoder reports the end of the first frame's display interval.
    EXPECT_EQ(timestamp, 10000);
    EXPECT_LT(pixels[0], 50);
    EXPECT_LT(pixels[1], 50);
    EXPECT_GT(pixels[2], 200);
    EXPECT_EQ(pixels[3], 255);
}

TEST_F(ApngConversionTest, ConvertsReportedImageWhenAvailable)
{
    const char* path = std::getenv("MEDIAPROXY_ISSUE_ONE_INPUT");
    if (path == nullptr) {
        GTEST_SKIP() << "Set MEDIAPROXY_ISSUE_ONE_INPUT to a local copy";
    }
    std::ifstream input(path, std::ios::binary);
    ASSERT_TRUE(input);
    const std::vector<char> raw{std::istreambuf_iterator<char>(input), {}};
    ASSERT_FALSE(raw.empty());
    const auto* begin = reinterpret_cast<const std::byte*>(raw.data());
    const std::vector<std::byte> body(begin, begin + raw.size());
    const auto result = mediaproxy::media::convert_media(body,
        mediaproxy::media::MimeType::image_png, false,
        mediaproxy::media::OutputFormat::webp,
        mediaproxy::media::ImageDimensions{3200, 3200});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    EXPECT_EQ(result.encoded_format, mediaproxy::media::OutputFormat::webp);
    ImagePtr decoded(vips_image_new_from_buffer(result.body.data(),
        result.body.size(), "", "n", -1, nullptr));
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    int pages = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES, &pages), 0);
    EXPECT_EQ(pages, 5);
}

TEST_F(ApngConversionTest, RejectsMalformedAndZeroTarget)
{
    EXPECT_FALSE(convert_apng_to_webp(
        ReadFixture("invalid-crc.png"), 4, 4));
    EXPECT_FALSE(convert_apng_to_webp(
        ReadFixture("over-none.png"), 0, 4));
}

} // namespace
