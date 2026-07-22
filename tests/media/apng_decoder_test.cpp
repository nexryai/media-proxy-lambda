#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/media/apng_compositor.hpp>
#include <mediaproxy/media/apng_decoder.hpp>
#include <openssl/sha.h>

namespace {

using mediaproxy::media::decode_apng_frames;
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

} // namespace
