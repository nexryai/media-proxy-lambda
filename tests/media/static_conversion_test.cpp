#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>
#include <mediaproxy/media/mime.hpp>
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
using mediaproxy::media::sniff_mime;

std::vector<std::byte> JxlContainerFixture()
{
    // libjxl/testdata at revision 873045a9c42ed60721756e26e2a6b32e17415205,
    // jxl/boxes/square-extended-size-container.jxl, SHA-256
    // c324d018c375f291230831fe27ba14e8ee39f3a1142d51b658a37227a6c1d850.
    constexpr std::array<unsigned char, 104> bytes{
        0x00, 0x00, 0x00, 0x0c, 0x4a, 0x58, 0x4c, 0x20,
        0x0d, 0x0a, 0x87, 0x0a, 0x00, 0x00, 0x00, 0x14,
        0x66, 0x74, 0x79, 0x70, 0x6a, 0x78, 0x6c, 0x20,
        0x00, 0x00, 0x00, 0x00, 0x6a, 0x78, 0x6c, 0x20,
        0x00, 0x00, 0x00, 0x01, 0x6a, 0x78, 0x6c, 0x63,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x48,
        0xff, 0x0a, 0x41, 0x06, 0x00, 0x12, 0x88, 0x02,
        0x00, 0xb4, 0x00, 0x55, 0x0f, 0x00, 0x00, 0xa8,
        0x50, 0x19, 0x65, 0xdc, 0xe0, 0xe5, 0x5c, 0xcf,
        0x97, 0x1f, 0x3a, 0x2c, 0xa6, 0x6d, 0x5c, 0x67,
        0x68, 0xab, 0x6d, 0x0b, 0x4b, 0x12, 0x45, 0xc6,
        0xb1, 0x49, 0x3a, 0x81, 0x43, 0x92, 0x48, 0xd2,
        0x60, 0x20, 0x99, 0xc1, 0x70, 0x5a, 0x25, 0x09,
    };
    std::vector<std::byte> result;
    result.reserve(bytes.size());
    for (const unsigned char byte : bytes) {
        result.push_back(static_cast<std::byte>(byte));
    }
    return result;
}

std::vector<std::byte> JxlCodestreamFixture()
{
    auto container = JxlContainerFixture();
    constexpr std::size_t codestream_offset = 48;
    return {container.begin() + codestream_offset, container.end()};
}

std::vector<std::byte> AnimatedJxlFixture()
{
    // libjxl/testdata revision and license are the same as the container
    // fixture above. This is jxl/spline_on_first_frame.jxl, SHA-256
    // 70f753c0de4ccc28859b3aa5c6810030784c1b6989831c81b9f72d4ca9776193.
    constexpr std::array<unsigned char, 76> bytes{
        0xff, 0x0a, 0x47, 0x40, 0x24, 0xd8, 0x63, 0x20,
        0x00, 0x00, 0x74, 0x00, 0x02, 0xe0, 0x28, 0x2a,
        0x2a, 0x46, 0xc6, 0x60, 0x10, 0x80, 0x5f, 0x00,
        0x00, 0x44, 0xe8, 0xcf, 0x84, 0x24, 0xe2, 0x8c,
        0x76, 0x02, 0xe8, 0x21, 0x96, 0x40, 0x70, 0x00,
        0x00, 0xd8, 0x63, 0x58, 0x00, 0x00, 0x70, 0x00,
        0x02, 0xe0, 0x20, 0x2a, 0x2a, 0x46, 0xc6, 0x48,
        0x00, 0x7e, 0x01, 0x00, 0x10, 0xa1, 0x5f, 0x12,
        0x12, 0x09, 0x44, 0x58, 0x40, 0xe1, 0x89, 0x25,
        0x10, 0x1c, 0x60, 0x08,
    };
    std::vector<std::byte> result;
    result.reserve(bytes.size());
    for (const unsigned char byte : bytes) {
        result.push_back(static_cast<std::byte>(byte));
    }
    return result;
}

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

std::vector<std::byte> Svg(std::string_view source)
{
    const auto* begin = reinterpret_cast<const std::byte*>(source.data());
    return {begin, begin + source.size()};
}

std::string PercentEncode(std::string_view source)
{
    constexpr std::string_view hex = "0123456789ABCDEF";
    std::string result;
    result.reserve(source.size() * 3);
    for (const unsigned char byte : source) {
        result.push_back('%');
        result.push_back(hex[byte >> 4U]);
        result.push_back(hex[byte & 0x0fU]);
    }
    return result;
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

void ExpectRedFirstPixel(VipsImage* image)
{
    double* raw_pixel = nullptr;
    int bands = 0;
    ASSERT_EQ(vips_getpoint(image, &raw_pixel, &bands, 0, 0, nullptr), 0)
        << vips_error_buffer();
    BufferPtr pixel(raw_pixel);
    ASSERT_GE(bands, 3);
    EXPECT_GT(raw_pixel[0], 180.0);
    EXPECT_LT(raw_pixel[1], 60.0);
    EXPECT_LT(raw_pixel[2], 60.0);
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

TEST_F(StaticConversionTest, DecodesJxlAndKeepsWebpOutput)
{
    const auto result = convert_static_image(JxlContainerFixture(),
        MimeType::image_jxl, OutputFormat::webp,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    EXPECT_EQ(sniff_mime(result.body), MimeType::image_webp);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 8);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 8);
}

TEST_F(StaticConversionTest, DecodesJxlAndKeepsAvifOutput)
{
    const auto result = convert_static_image(JxlContainerFixture(),
        MimeType::image_jxl, OutputFormat::avif,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    EXPECT_EQ(sniff_mime(result.body), MimeType::image_avif);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 8);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 8);
}

TEST_F(StaticConversionTest, DecodesBareJxlCodestream)
{
    const auto result = convert_static_image(JxlCodestreamFixture(),
        MimeType::image_jxl, OutputFormat::webp,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    EXPECT_EQ(sniff_mime(result.body), MimeType::image_webp);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 8);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 8);
}

TEST_F(StaticConversionTest, DecodesFirstDisplayedJxlFrame)
{
    const auto result = convert_static_image(AnimatedJxlFixture(),
        MimeType::image_jxl, OutputFormat::webp,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_GT(vips_image_get_width(decoded.get()), 0);
    EXPECT_GT(vips_image_get_height(decoded.get()), 0);
    EXPECT_EQ(vips_image_get_typeof(decoded.get(), VIPS_META_N_PAGES), 0U);
}

TEST_F(StaticConversionTest, RejectsTruncatedJxl)
{
    auto input = JxlContainerFixture();
    input.resize(52);
    EXPECT_FALSE(convert_static_image(input, MimeType::image_jxl,
        OutputFormat::webp, ImageDimensions{320, 320}));
}

TEST_F(StaticConversionTest, RendersSvgWithEmbeddedFont)
{
    const auto input = Svg(
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="64" height="32"><rect width="64" height="32" fill="#2478c8"/><text x="3" y="24" font-size="18" fill="white">日本語</text></svg>)");
    const auto result = convert_static_image(input,
        MimeType::image_svg_xml, OutputFormat::webp,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 64);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 32);
}

TEST_F(StaticConversionTest, RejectsSvgDoctype)
{
    const auto input = Svg(
        R"(<!DOCTYPE svg><svg xmlns="http://www.w3.org/2000/svg" width="1" height="1"/>)");
    EXPECT_FALSE(convert_static_image(input, MimeType::image_svg_xml,
        OutputFormat::webp, ImageDimensions{320, 320}));
}

TEST_F(StaticConversionTest, IgnoresSvgExternalImageHref)
{
    const std::string source =
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="8" height="6"><rect width="8" height="6" fill="red"/><image href=")"
        + std::string(MEDIAPROXY_SOURCE_DIR)
        + R"(/tests/fixtures/media/apng/palette-alpha.png" width="8" height="6"/></svg>)";
    const auto input = Svg(source);
    const auto result = convert_static_image(input,
        MimeType::image_svg_xml, OutputFormat::webp,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 8);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 6);
    ExpectRedFirstPixel(decoded.get());
}

TEST_F(StaticConversionTest, RejectsOversizedSvgCanvas)
{
    const auto input = Svg(
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="7681" height="1"/>)");
    EXPECT_FALSE(convert_static_image(input, MimeType::image_svg_xml,
        OutputFormat::webp, ImageDimensions{320, 320}));
}

TEST_F(StaticConversionTest, RejectsAutoDerivedOversizedSvgCanvas)
{
    const auto input = Svg(
        R"(<svg ls="3.2"><path d="M-7-96 6 0 07 4E7"/></svg>)");
    EXPECT_FALSE(convert_static_image(input, MimeType::image_svg_xml,
        OutputFormat::webp, ImageDimensions{320, 320}));
}

TEST_F(StaticConversionTest, SkipsSvgDataUrlsAfterCountLimit)
{
    constexpr std::string_view transparent =
        "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxIiBoZWlnaHQ9IjEiLz4=";
    constexpr std::string_view blue =
        "PHN2ZyB4bWxucz0iaHR0cDovL3d3dy53My5vcmcvMjAwMC9zdmciIHdpZHRoPSIxIiBoZWlnaHQ9IjEiPjxyZWN0IHdpZHRoPSIxIiBoZWlnaHQ9IjEiIGZpbGw9ImJsdWUiLz48L3N2Zz4=";
    std::string source =
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1"><rect width="1" height="1" fill="red"/>)";
    for (std::size_t index = 0; index < 128; ++index) {
        source += R"(<image width="1" height="1" href="data:image/svg+xml;base64,)";
        source += transparent;
        source += R"("/>)";
    }
    source += R"(<image width="1" height="1" href="data:image/svg+xml;base64,)";
    source += blue;
    source += R"("/></svg>)";

    const auto result = convert_static_image(Svg(source),
        MimeType::image_svg_xml, OutputFormat::webp,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    ExpectRedFirstPixel(decoded.get());
}

TEST_F(StaticConversionTest, RejectsSvgOverNodeLimit)
{
    std::string source =
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1">)";
    for (std::size_t index = 0; index < 100'001; ++index) {
        source += "<path/>";
    }
    source += "</svg>";
    EXPECT_FALSE(convert_static_image(Svg(source),
        MimeType::image_svg_xml, OutputFormat::webp,
        ImageDimensions{320, 320}));
}

TEST_F(StaticConversionTest, SkipsNestedSvgOverNodeLimit)
{
    std::string nested =
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1">)";
    for (std::size_t index = 0; index < 100'001; ++index) {
        nested += "<path/>";
    }
    nested += "</svg>";
    const std::string source =
        R"(<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1"><rect width="1" height="1" fill="red"/><image width="1" height="1" href="data:image/svg+xml,)"
        + PercentEncode(nested) + R"("/></svg>)";

    const auto result = convert_static_image(Svg(source),
        MimeType::image_svg_xml, OutputFormat::webp,
        ImageDimensions{320, 320});
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    const ImagePtr decoded = Load(result.body);
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    ExpectRedFirstPixel(decoded.get());
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
