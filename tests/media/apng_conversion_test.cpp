#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>
#include <mediaproxy/media/apng_conversion.hpp>
#include <mediaproxy/media/apng_compositor.hpp>
#include <mediaproxy/media/apng_decoder.hpp>
#include <mediaproxy/media/conversion.hpp>
#include <mediaproxy/media/vips_runtime.hpp>
#include <openssl/sha.h>
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

std::string Sha256(std::span<const std::byte> input)
{
    std::array<std::uint8_t, SHA256_DIGEST_LENGTH> digest{};
    EXPECT_EQ(::SHA256(reinterpret_cast<const std::uint8_t*>(input.data()),
                  input.size(), digest.data()),
        digest.data());
    constexpr char hex[] = "0123456789abcdef";
    std::string output;
    output.reserve(digest.size() * 2);
    for (const auto byte : digest) {
        output.push_back(hex[byte >> 4U]);
        output.push_back(hex[byte & 0x0fU]);
    }
    return output;
}

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
    EXPECT_EQ(timestamp, 5000);
    EXPECT_LT(pixels[0], 50);
    EXPECT_LT(pixels[1], 50);
    EXPECT_GT(pixels[2], 200);
    EXPECT_EQ(pixels[3], 255);
}

TEST_F(ApngConversionTest, EncodesIssueOneLayoutWithPinnedLosslessBytes)
{
    const auto input = ReadFixture("issue-1-first-frame.png");
    const auto result = convert_apng_to_webp(input, 4, 4);
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    EXPECT_EQ(Sha256(result.body),
        "2a86dd357ccc20bffd1bad2488a2c39b9d155a5678af047836e82e8025c0b475");
    const auto enlarged = convert_apng_to_webp(input, 128, 128);
    ASSERT_TRUE(enlarged) << static_cast<int>(enlarged.error);
    EXPECT_EQ(Sha256(enlarged.body),
        "e780f38c1ce1bd1ed51ff9aff8b3dc08a8f47f723ca4057a83476c383d22d293");

    const auto decoded = mediaproxy::media::decode_apng_frames(input);
    ASSERT_TRUE(decoded);
    std::vector<std::byte> canvas(4U * 4U * 4U, std::byte{0});
    const WebPData webp{
        .bytes = reinterpret_cast<const std::uint8_t*>(result.body.data()),
        .size = result.body.size(),
    };
    using DecoderPtr = std::unique_ptr<WebPAnimDecoder,
        decltype(&WebPAnimDecoderDelete)>;
    DecoderPtr decoder(WebPAnimDecoderNew(&webp, nullptr),
        &WebPAnimDecoderDelete);
    ASSERT_NE(decoder, nullptr);
    constexpr std::array<std::string_view, 5> expected_frame_hashes{
        "40ec232d5d5d799b4ef08c2459b1109491948123c89ebf704636d85aede601d6",
        "fec0f57de0b19bc7dacb5b0fc3de7b56fc68dfdbeeebc8f9f4c506bf6e821c77",
        "83fd42e005dae0b86d822aca42841f845589041c4031397f06f631b814991e1e",
        "f4e50f9c68e78511ed9c026c4c3a109c74171bc8faa0ee38a1bdd4da526b18dc",
        "d0462ef3919b0811d24ec0e1f57f64ec30976722fb69f4b662c6928e65a9c052"};
    ASSERT_EQ(decoded.frames.size(), expected_frame_hashes.size());
    for (std::size_t frame_index = 0; frame_index < decoded.frames.size();
         ++frame_index) {
        const auto& frame = decoded.frames[frame_index];
        const auto composed = mediaproxy::media::compose_apng_frame(canvas,
            decoded.canvas_width, decoded.canvas_height, frame.control,
            frame.rgba);
        ASSERT_TRUE(composed);
        std::uint8_t* pixels = nullptr;
        int timestamp = 0;
        ASSERT_EQ(WebPAnimDecoderGetNext(decoder.get(), &pixels, &timestamp), 1);
        ASSERT_NE(pixels, nullptr);
        EXPECT_EQ(Sha256(composed.displayed_rgba),
            expected_frame_hashes[frame_index]);
        EXPECT_EQ(Sha256(std::as_bytes(std::span{pixels,
                      composed.displayed_rgba.size()})),
            expected_frame_hashes[frame_index]);
    }
    EXPECT_EQ(WebPAnimDecoderHasMoreFrames(decoder.get()), 0);
}

TEST_F(ApngConversionTest, PreservesIssueTwoColorsAndTiming)
{
    const auto input = ReadFixture("issue-2-color-timing.png");
    const auto source = mediaproxy::media::decode_apng_frames(input);
    ASSERT_TRUE(source);
    ASSERT_EQ(source.frames.size(), 3U);
    const auto result = convert_apng_to_webp(input, 4, 4);
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    const WebPData webp{
        .bytes = reinterpret_cast<const std::uint8_t*>(result.body.data()),
        .size = result.body.size(),
    };
    using DecoderPtr = std::unique_ptr<WebPAnimDecoder,
        decltype(&WebPAnimDecoderDelete)>;
    DecoderPtr decoder(WebPAnimDecoderNew(&webp, nullptr),
        &WebPAnimDecoderDelete);
    ASSERT_NE(decoder, nullptr);

    constexpr std::array<std::string_view, 3> expected_frame_hashes{
        "5b7080fbbbcf73befa36932de9bcfec0023ce2ac59635fde3f804023a430243f",
        "5c0517effd8e1c3aa0c656bff757c02abb9269158a84ceb215605c8bf223b83d",
        "83fd42e005dae0b86d822aca42841f845589041c4031397f06f631b814991e1e"};
    std::vector<std::byte> canvas(4U * 4U * 4U, std::byte{0});
    for (std::size_t index = 0; index < source.frames.size(); ++index) {
        const auto& frame = source.frames[index];
        const auto composed = mediaproxy::media::compose_apng_frame(canvas,
            source.canvas_width, source.canvas_height, frame.control,
            frame.rgba);
        ASSERT_TRUE(composed);
        EXPECT_EQ(Sha256(composed.displayed_rgba),
            expected_frame_hashes[index]);
        std::uint8_t* pixels = nullptr;
        int timestamp = 0;
        ASSERT_EQ(WebPAnimDecoderGetNext(decoder.get(), &pixels, &timestamp), 1);
        ASSERT_NE(pixels, nullptr);
        EXPECT_EQ(Sha256(std::as_bytes(std::span{pixels,
                      composed.displayed_rgba.size()})),
            expected_frame_hashes[index]);
        if (index < 2U) {
            EXPECT_EQ(timestamp, index == 0U ? 5000 : 6000);
        }
    }
    EXPECT_EQ(WebPAnimDecoderHasMoreFrames(decoder.get()), 0);
}

TEST_F(ApngConversionTest, PaletteAlphaSurvivesWebpEncoding)
{
    const auto result = convert_apng_to_webp(
        ReadFixture("palette-alpha.png"), 2, 2);
    ASSERT_TRUE(result) << static_cast<int>(result.error);
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
    int timestamp = 0;
    ASSERT_EQ(WebPAnimDecoderGetNext(decoder.get(), &pixels, &timestamp), 1);
    ASSERT_EQ(WebPAnimDecoderGetNext(decoder.get(), &pixels, &timestamp), 1);
    ASSERT_NE(pixels, nullptr);
    // The second input frame has a half-transparent green palette entry.
    EXPECT_NEAR(pixels[7], 128, 2);
}

TEST_F(ApngConversionTest, UsesPrecedingFrameDelays)
{
    const auto result = convert_apng_to_webp(
        ReadFixture("varying-delays-loop.png"), 4, 4);
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    const WebPData webp{
        .bytes = reinterpret_cast<const std::uint8_t*>(result.body.data()),
        .size = result.body.size(),
    };
    using DecoderPtr = std::unique_ptr<WebPAnimDecoder,
        decltype(&WebPAnimDecoderDelete)>;
    DecoderPtr decoder(WebPAnimDecoderNew(&webp, nullptr),
        &WebPAnimDecoderDelete);
    ASSERT_NE(decoder, nullptr);
    WebPAnimInfo info{};
    ASSERT_EQ(WebPAnimDecoderGetInfo(decoder.get(), &info), 1);
    EXPECT_EQ(info.frame_count, 4);
    EXPECT_EQ(info.loop_count, 0);

    constexpr std::array<int, 3> expected_end_timestamps{100, 200, 450};
    for (const int expected : expected_end_timestamps) {
        std::uint8_t* pixels = nullptr;
        int timestamp = 0;
        ASSERT_EQ(WebPAnimDecoderGetNext(decoder.get(), &pixels, &timestamp), 1);
        EXPECT_EQ(timestamp, expected);
    }
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
    const auto source = mediaproxy::media::decode_apng_frames(body);
    ASSERT_TRUE(source);
    std::vector<std::byte> canvas(
        static_cast<std::size_t>(source.canvas_width)
            * source.canvas_height * 4U,
        std::byte{0});
    const WebPData webp{
        .bytes = reinterpret_cast<const std::uint8_t*>(result.body.data()),
        .size = result.body.size(),
    };
    using DecoderPtr = std::unique_ptr<WebPAnimDecoder,
        decltype(&WebPAnimDecoderDelete)>;
    DecoderPtr decoder(WebPAnimDecoderNew(&webp, nullptr),
        &WebPAnimDecoderDelete);
    ASSERT_NE(decoder, nullptr);
    ASSERT_EQ(source.frames.size(), 5U);
    for (std::size_t index = 0; index < source.frames.size(); ++index) {
        std::uint8_t* pixels = nullptr;
        int timestamp = 0;
        ASSERT_EQ(WebPAnimDecoderGetNext(decoder.get(), &pixels, &timestamp), 1);
        if (index + 1U < source.frames.size()) {
            EXPECT_EQ(timestamp, static_cast<int>(index + 1U) * 5000);
        }
        const auto composed = mediaproxy::media::compose_apng_frame(canvas,
            source.canvas_width, source.canvas_height,
            source.frames[index].control, source.frames[index].rgba);
        ASSERT_TRUE(composed);
        EXPECT_EQ(Sha256(std::as_bytes(std::span{pixels,
                      composed.displayed_rgba.size()})),
            Sha256(composed.displayed_rgba));
    }
    EXPECT_EQ(WebPAnimDecoderHasMoreFrames(decoder.get()), 0);
}

TEST_F(ApngConversionTest, RejectsMalformedAndZeroTarget)
{
    EXPECT_FALSE(convert_apng_to_webp(
        ReadFixture("invalid-crc.png"), 4, 4));
    EXPECT_FALSE(convert_apng_to_webp(
        ReadFixture("over-none.png"), 0, 4));
}

} // namespace
