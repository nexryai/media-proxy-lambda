#include <cstddef>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/media/apng.hpp>

namespace {

using mediaproxy::media::ApngClassification;
using mediaproxy::media::ApngParseError;
using mediaproxy::media::classify_apng;
using mediaproxy::media::parse_apng;

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

TEST(ApngClassification, PreservesFixedOffsetChecks)
{
    EXPECT_EQ(classify_apng(ReadFixture("over-none.png")),
        ApngClassification::animated);
    EXPECT_EQ(classify_apng(ReadFixture("palette-static.png")),
        ApngClassification::palette);
    EXPECT_EQ(classify_apng(ReadFixture("detection-length-41.png")),
        ApngClassification::not_apng);
    EXPECT_EQ(classify_apng(
                  ReadFixture("animation-control-other-offset.png")),
        ApngClassification::not_apng);
}

TEST(ApngParser, ReadsCanvasAnimationAndFrameControls)
{
    const auto parsed = parse_apng(ReadFixture("over-none.png"));
    ASSERT_TRUE(parsed) << static_cast<int>(parsed.error);
    EXPECT_EQ(parsed.canvas_width, 4U);
    EXPECT_EQ(parsed.canvas_height, 4U);
    EXPECT_EQ(parsed.declared_frames, 3U);
    ASSERT_EQ(parsed.frames.size(), 3U);
    EXPECT_EQ(parsed.frames[1].blend, 1U);
    EXPECT_EQ(parsed.frames[1].dispose, 0U);
    EXPECT_EQ(parsed.frames[1].delay_numerator, 1U);
    EXPECT_EQ(parsed.frames[1].delay_denominator, 10U);
}

struct InvalidCase {
    const char* file;
    ApngParseError error;
};

class ApngInvalidParserTest
    : public testing::TestWithParam<InvalidCase> {
};

TEST_P(ApngInvalidParserTest, RejectsMalformedControlData)
{
    const auto parsed = parse_apng(ReadFixture(GetParam().file));
    EXPECT_FALSE(parsed);
    EXPECT_EQ(parsed.error, GetParam().error);
}

INSTANTIATE_TEST_SUITE_P(Fixtures, ApngInvalidParserTest,
    testing::Values(
        InvalidCase{"invalid-out-of-bounds-right.png",
            ApngParseError::frame_rectangle},
        InvalidCase{"invalid-out-of-bounds-bottom.png",
            ApngParseError::frame_rectangle},
        InvalidCase{"invalid-zero-denominator.png",
            ApngParseError::delay_denominator},
        InvalidCase{"invalid-unknown-blend.png",
            ApngParseError::blend_operation},
        InvalidCase{"invalid-unknown-dispose.png",
            ApngParseError::dispose_operation},
        InvalidCase{"invalid-crc.png", ApngParseError::crc},
        InvalidCase{"invalid-truncated.png",
            ApngParseError::truncated_chunk},
        InvalidCase{"invalid-length-overflow.png",
            ApngParseError::chunk_length}));

} // namespace
