#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/media/apng_compositor.hpp>
#include <mediaproxy/media/apng_decoder.hpp>
#include <openssl/sha.h>
#include <yyjson.h>

namespace {

using mediaproxy::media::decode_apng_frames;
using mediaproxy::media::apng_frame_timestamp_ms;
using mediaproxy::media::compose_apng_frame;

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

std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> LoadManifest()
{
    const std::string path = std::string{MEDIAPROXY_SOURCE_DIR}
        + "/tests/fixtures/media/apng/manifest.json";
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input) << path;
    std::string json{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    yyjson_read_err error{};
    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> document(
        yyjson_read_opts(
            json.data(), json.size(), YYJSON_READ_NOFLAG, nullptr, &error),
        &yyjson_doc_free);
    EXPECT_NE(document, nullptr) << error.msg;
    return document;
}

TEST(ApngDecoder, DecodesEveryFrameToStraightRgba)
{
    const auto decoded = decode_apng_frames(ReadFixture("over-none.png"));
    ASSERT_TRUE(decoded) << static_cast<int>(decoded.error);
    EXPECT_EQ(decoded.canvas_width, 4U);
    EXPECT_EQ(decoded.canvas_height, 4U);
    ASSERT_EQ(decoded.frames.size(), 3U);

    const auto& base = decoded.frames[0];
    EXPECT_EQ(base.rgba.size(), 4U * 4U * 4U);
    for (std::size_t offset = 0; offset < base.rgba.size(); offset += 4) {
        EXPECT_EQ(base.rgba[offset], std::byte{0});
        EXPECT_EQ(base.rgba[offset + 1], std::byte{0});
        EXPECT_EQ(base.rgba[offset + 2], std::byte{255});
        EXPECT_EQ(base.rgba[offset + 3], std::byte{255});
    }

    const auto& overlay = decoded.frames[1];
    EXPECT_EQ(overlay.control.width, 2U);
    EXPECT_EQ(overlay.control.height, 2U);
    EXPECT_EQ(overlay.control.x_offset, 1U);
    EXPECT_EQ(overlay.control.y_offset, 1U);
    for (std::size_t offset = 0; offset < overlay.rgba.size(); offset += 4) {
        EXPECT_EQ(overlay.rgba[offset], std::byte{255});
        EXPECT_EQ(overlay.rgba[offset + 1], std::byte{0});
        EXPECT_EQ(overlay.rgba[offset + 2], std::byte{0});
        EXPECT_EQ(overlay.rgba[offset + 3], std::byte{128});
    }
}

TEST(ApngDecoder, RejectsMalformedFrameStreamBeforeDecode)
{
    const auto decoded =
        decode_apng_frames(ReadFixture("invalid-crc.png"));
    EXPECT_FALSE(decoded);
}

TEST(ApngDecoder, MatchesCheckedInFullCanvasFrameHashes)
{
    auto decoded = decode_apng_frames(ReadFixture("over-none.png"));
    ASSERT_TRUE(decoded) << static_cast<int>(decoded.error);
    ASSERT_EQ(decoded.frames.size(), 3U);
    auto canvas = decoded.frames.front().rgba;

    const auto first = compose_apng_frame(canvas, decoded.canvas_width,
        decoded.canvas_height, decoded.frames[1].control,
        decoded.frames[1].rgba);
    ASSERT_TRUE(first);
    EXPECT_EQ(Sha256(first.displayed_rgba),
        "675630baa97886ee49197744fb82c38fdfaf6bb99c275c3a9117f747bdbcf928");
    EXPECT_EQ(Sha256(canvas),
        "675630baa97886ee49197744fb82c38fdfaf6bb99c275c3a9117f747bdbcf928");

    const auto second = compose_apng_frame(canvas, decoded.canvas_width,
        decoded.canvas_height, decoded.frames[2].control,
        decoded.frames[2].rgba);
    ASSERT_TRUE(second);
    EXPECT_EQ(Sha256(second.displayed_rgba),
        "ce14f64dd6a5a5314eed05c4399d98853c0b3208422894ea56f6ef163426c060");
    EXPECT_EQ(Sha256(canvas),
        "ce14f64dd6a5a5314eed05c4399d98853c0b3208422894ea56f6ef163426c060");
}

TEST(ApngDecoder, MatchesAllNonPaletteGoldenFrameTransitions)
{
    const auto manifest = LoadManifest();
    ASSERT_NE(manifest, nullptr);
    yyjson_val* fixtures =
        yyjson_obj_get(yyjson_doc_get_root(manifest.get()), "fixtures");
    ASSERT_TRUE(yyjson_is_arr(fixtures));

    std::size_t fixture_index = 0;
    std::size_t fixture_count = 0;
    yyjson_val* fixture = nullptr;
    yyjson_arr_foreach(
        fixtures, fixture_index, fixture_count, fixture)
    {
        const char* classification = yyjson_get_str(
            yyjson_obj_get(fixture, "expectedClassification"));
        if (classification == nullptr
            || std::string_view{classification} != "apng-nonpalette") {
            continue;
        }
        const char* id = yyjson_get_str(yyjson_obj_get(fixture, "id"));
        const char* file = yyjson_get_str(yyjson_obj_get(fixture, "file"));
        ASSERT_NE(id, nullptr);
        ASSERT_NE(file, nullptr);
        SCOPED_TRACE(id);

        auto decoded = decode_apng_frames(ReadFixture(file));
        ASSERT_TRUE(decoded) << static_cast<int>(decoded.error);
        ASSERT_FALSE(decoded.frames.empty());
        auto canvas = decoded.frames.front().rgba;
        yyjson_val* emitted = yyjson_obj_get(fixture, "emittedFrames");
        ASSERT_TRUE(yyjson_is_arr(emitted));
        EXPECT_EQ(yyjson_arr_size(emitted), decoded.frames.size() - 1);

        std::size_t frame_index = 0;
        std::size_t frame_count = 0;
        yyjson_val* expected = nullptr;
        yyjson_arr_foreach(emitted, frame_index, frame_count, expected)
        {
            const auto callback = static_cast<std::size_t>(yyjson_get_uint(
                yyjson_obj_get(expected, "callbackNumber")));
            ASSERT_LT(callback, decoded.frames.size());
            const auto& frame = decoded.frames[callback];
            const auto composed = compose_apng_frame(canvas,
                decoded.canvas_width, decoded.canvas_height,
                frame.control, frame.rgba);
            ASSERT_TRUE(composed);

            const char* displayed_hash = yyjson_get_str(
                yyjson_obj_get(expected, "displayedRgbaSha256"));
            const char* next_hash = yyjson_get_str(
                yyjson_obj_get(expected, "nextCanvasRgbaSha256"));
            ASSERT_NE(displayed_hash, nullptr);
            ASSERT_NE(next_hash, nullptr);
            EXPECT_EQ(Sha256(composed.displayed_rgba), displayed_hash);
            EXPECT_EQ(Sha256(canvas), next_hash);
            EXPECT_EQ(apng_frame_timestamp_ms(
                          static_cast<std::uint32_t>(callback),
                          frame.control.delay_numerator,
                          frame.control.delay_denominator),
                yyjson_get_sint(
                    yyjson_obj_get(expected, "timestampMs")));
        }
    }
}

} // namespace
