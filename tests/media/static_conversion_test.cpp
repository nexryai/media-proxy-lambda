#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>
#include <mediaproxy/media/static_conversion.hpp>
#include <mediaproxy/media/vips_runtime.hpp>
#include <vips/vips.h>
#include <webp/encode.h>

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
using mediaproxy::media::MimeType;
using mediaproxy::media::OutputFormat;
using mediaproxy::media::convert_static_image;
using mediaproxy::media::initialize_vips;

std::vector<std::byte> MakeWebp(int width, int height)
{
    std::vector<std::uint8_t> pixels(
        static_cast<std::size_t>(width * height * 4), 0);
    std::uint8_t* raw_buffer = nullptr;
    const std::size_t size = WebPEncodeLosslessRGBA(
        pixels.data(), width, height, width * 4, &raw_buffer);
    EXPECT_GT(size, 0U);
    std::unique_ptr<std::uint8_t, decltype(&WebPFree)> buffer(
        raw_buffer, &WebPFree);
    const auto* bytes = reinterpret_cast<const std::byte*>(buffer.get());
    return {bytes, bytes + size};
}

std::vector<std::byte> MakePng(int width, int height)
{
    VipsImage* raw_image = nullptr;
    EXPECT_EQ(vips_black(&raw_image, width, height, nullptr), 0);
    ImagePtr image(raw_image);
    void* raw_buffer = nullptr;
    std::size_t size = 0;
    EXPECT_EQ(vips_pngsave_buffer(image.get(), &raw_buffer, &size, nullptr), 0)
        << vips_error_buffer();
    BufferPtr buffer(raw_buffer);
    const auto* bytes = static_cast<const std::byte*>(buffer.get());
    return {bytes, bytes + size};
}

void AppendU16(std::vector<std::byte>& output, std::uint16_t value)
{
    output.push_back(static_cast<std::byte>(value & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}

void AppendU32(std::vector<std::byte>& output, std::uint32_t value)
{
    output.push_back(static_cast<std::byte>(value & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 16U) & 0xffU));
    output.push_back(static_cast<std::byte>((value >> 24U) & 0xffU));
}

std::vector<std::byte> MakeIco(
    const std::vector<std::vector<std::byte>>& entries)
{
    std::vector<std::byte> output;
    AppendU16(output, 0);
    AppendU16(output, 1);
    AppendU16(output, static_cast<std::uint16_t>(entries.size()));
    std::uint32_t offset = static_cast<std::uint32_t>(6 + entries.size() * 16);
    for (const auto& entry : entries) {
        output.insert(output.end(), 8, std::byte{0});
        AppendU32(output, static_cast<std::uint32_t>(entry.size()));
        AppendU32(output, offset);
        offset += static_cast<std::uint32_t>(entry.size());
    }
    for (const auto& entry : entries) {
        output.insert(output.end(), entry.begin(), entry.end());
    }
    return output;
}

std::vector<std::byte> MakeAnimatedWebp(int width, int page_height)
{
    VipsImage* raw_first = nullptr;
    VipsImage* raw_second = nullptr;
    EXPECT_EQ(vips_black(&raw_first, width, page_height, nullptr), 0);
    EXPECT_EQ(vips_black(&raw_second, width, page_height, nullptr), 0);
    ImagePtr first(raw_first);
    ImagePtr second(raw_second);

    VipsImage* raw_joined = nullptr;
    EXPECT_EQ(vips_join(first.get(), second.get(), &raw_joined,
                  VIPS_DIRECTION_VERTICAL, nullptr),
        0)
        << vips_error_buffer();
    ImagePtr joined(raw_joined);
    vips_image_set_int(joined.get(), VIPS_META_PAGE_HEIGHT, page_height);
    vips_image_set_int(joined.get(), VIPS_META_N_PAGES, 2);

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

ImagePtr Load(std::span<const std::byte> body)
{
    return ImagePtr(vips_image_new_from_buffer(
        body.data(), body.size(), "", nullptr));
}

class StaticConversionTest : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(initialize_vips()) << vips_error_buffer();
    }
};

TEST_F(StaticConversionTest, ResizesAndEncodesLossyWebp)
{
    const auto input = MakeWebp(100, 80);
    const auto result = convert_static_image(
        input, MimeType::image_webp, OutputFormat::webp,
        ImageDimensions{50, 40});
    ASSERT_TRUE(result);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 50);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 40);
}

TEST_F(StaticConversionTest, EncodesAvifWithConfiguredBackend)
{
    const auto input = MakeWebp(8, 6);
    const auto result = convert_static_image(
        input, MimeType::image_webp, OutputFormat::avif,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 8);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 6);
}

TEST_F(StaticConversionTest, LoadsAllPagesButEncodesOnlyFirstPage)
{
    const auto input = MakeAnimatedWebp(12, 7);
    const auto result = convert_static_image(
        input, MimeType::image_webp, OutputFormat::webp,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 12);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 7);
}

TEST_F(StaticConversionTest, RejectsEmptyAndMalformedInput)
{
    EXPECT_FALSE(convert_static_image(
        {}, MimeType::image_webp, OutputFormat::webp,
        ImageDimensions{320, 320}));
    const std::vector malformed{std::byte{0}, std::byte{1}};
    EXPECT_FALSE(convert_static_image(
        malformed, MimeType::image_webp, OutputFormat::webp,
        ImageDimensions{320, 320}));
}

TEST_F(StaticConversionTest, IcoFallbackUsesFirstDecodableEntry)
{
    const std::vector corrupt{std::byte{0}, std::byte{1}};
    const auto ico = MakeIco({corrupt, MakePng(13, 9), MakePng(32, 24)});
    const auto result = convert_static_image(
        ico, MimeType::image_x_icon, OutputFormat::webp,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 13);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 9);
}

TEST_F(StaticConversionTest, IcoFallbackRejectsInvalidDirectory)
{
    auto ico = MakeIco({MakePng(8, 8)});
    ico[18] = std::byte{0xff};
    ico[19] = std::byte{0xff};
    ico[20] = std::byte{0xff};
    ico[21] = std::byte{0x7f};
    EXPECT_FALSE(convert_static_image(
        ico, MimeType::image_ico, OutputFormat::webp,
        ImageDimensions{320, 320}));
}

} // namespace
