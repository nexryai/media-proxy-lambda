#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/media/apng_decoder.hpp>

namespace {

using mediaproxy::media::decode_apng_frames;

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

} // namespace
