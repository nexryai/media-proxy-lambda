#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <tuple>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>
#include <mediaproxy/http/query.hpp>
#include <mediaproxy/media/conversion.hpp>
#include <mediaproxy/media/vips_runtime.hpp>
#include <vips/vips.h>

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
using mediaproxy::http::PreferredOutput;
using mediaproxy::http::parse_query;
using mediaproxy::http::select_media_options;
using mediaproxy::media::ImageDimensions;
using mediaproxy::media::MimeType;
using mediaproxy::media::OutputFormat;
using mediaproxy::media::convert_media;
using mediaproxy::media::initialize_vips;

struct Target {
    std::string_view name;
    std::string_view query;
    std::string_view extension;
    OutputFormat output;
};

constexpr std::array targets{
    Target{"avatar", "avatar=1", "avif", OutputFormat::avif},
    Target{"emoji", "emoji=1", "avif", OutputFormat::avif},
    Target{"preview", "preview=1", "webp", OutputFormat::webp},
    Target{"badge", "badge=1", "avif", OutputFormat::avif},
    Target{"thumbnail", "thumbnail=1", "webp", OutputFormat::webp},
    Target{"ticker", "ticker=1", "avif", OutputFormat::avif},
    Target{"default", "", "webp", OutputFormat::webp},
};

struct Fixture {
    std::string_view filename;
    MimeType mime;
    std::array<ImageDimensions, targets.size()> expected;
};

constexpr std::array fixtures{
    Fixture{"1500 x749.jpg", MimeType::image_jpeg,
        {{{320, 160}, {700, 350}, {200, 100}, {96, 48}, {500, 250},
            {64, 32}, {1500, 749}}}},
    Fixture{"1500x843.jpg", MimeType::image_jpeg,
        {{{320, 180}, {700, 393}, {200, 112}, {96, 54}, {500, 281},
            {64, 36}, {1500, 843}}}},
    Fixture{"602 x602.jpg", MimeType::image_jpeg,
        {{{320, 320}, {128, 128}, {200, 200}, {96, 96}, {400, 400},
            {64, 64}, {602, 602}}}},
    Fixture{"7680x4320_1.png", MimeType::image_png,
        {{{320, 180}, {700, 394}, {200, 113}, {96, 54}, {500, 281},
            {64, 36}, {3200, 1800}}}},
    Fixture{"7680x4320_1.avif", MimeType::image_avif,
        {{{320, 180}, {700, 394}, {200, 113}, {96, 54}, {500, 281},
            {64, 36}, {3200, 1800}}}},
    Fixture{"7680x4320_1.webp", MimeType::image_webp,
        {{{320, 180}, {700, 394}, {200, 113}, {96, 54}, {500, 281},
            {64, 36}, {3200, 1800}}}},
    Fixture{"7680x4320_lossless.avif", MimeType::image_avif,
        {{{320, 180}, {700, 394}, {200, 113}, {96, 54}, {500, 281},
            {64, 36}, {3200, 1800}}}},
    Fixture{"7680x4320_lossless.webp", MimeType::image_webp,
        {{{320, 180}, {700, 394}, {200, 113}, {96, 54}, {500, 281},
            {64, 36}, {3200, 1800}}}},
};

std::vector<std::byte> ReadFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) {
        ADD_FAILURE() << "Unable to open " << path;
        return {};
    }
    const std::streampos end = input.tellg();
    if (end <= 0) {
        ADD_FAILURE() << "Invalid fixture size for " << path;
        return {};
    }
    std::vector<std::byte> bytes(static_cast<std::size_t>(end));
    input.seekg(0);
    input.read(reinterpret_cast<char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        ADD_FAILURE() << "Unable to read " << path;
        return {};
    }
    return bytes;
}

bool WriteFile(
    const std::string& path,
    std::span<const std::byte> bytes)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(reinterpret_cast<const char*>(bytes.data()),
        static_cast<std::streamsize>(bytes.size()));
    return static_cast<bool>(output);
}

OutputFormat ToOutputFormat(PreferredOutput output)
{
    return output == PreferredOutput::avif
        ? OutputFormat::avif
        : OutputFormat::webp;
}

using FixtureParameter = std::tuple<std::size_t, std::size_t>;

class ResizeFixtureTest : public testing::TestWithParam<FixtureParameter> {
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(initialize_vips()) << vips_error_buffer();
    }
};

TEST_P(ResizeFixtureTest, WritesSelectorResultAtExpectedDimensions)
{
    const std::filesystem::path source_root{MEDIAPROXY_SOURCE_DIR};
    const auto result_directory = source_root / "tests/results/resize";
    std::error_code directory_error;
    std::filesystem::create_directories(result_directory, directory_error);
    ASSERT_FALSE(directory_error) << directory_error.message();

    const auto [fixture_index, target_index] = GetParam();
    const auto& fixture = fixtures[fixture_index];
    const auto& target = targets[target_index];
    SCOPED_TRACE(fixture.filename);
    SCOPED_TRACE(target.name);
    const std::string fixture_path = std::string{MEDIAPROXY_SOURCE_DIR}
        + "/tests/fixtures/media/resize/" + std::string(fixture.filename);
    const auto input = ReadFile(fixture_path);
    ASSERT_FALSE(input.empty());

    const auto options = select_media_options(parse_query(target.query));
    const auto requested_output = ToOutputFormat(options.preferred_output);
    EXPECT_EQ(requested_output, target.output);

    const auto result = convert_media(input, fixture.mime,
        options.force_static, requested_output,
        {options.width_limit, options.height_limit});
    ASSERT_TRUE(result) << "Conversion failed with error "
                        << static_cast<int>(result.error) << ": "
                        << vips_error_buffer();
    EXPECT_EQ(result.encoded_format, target.output);

    const std::string output_path = result_directory.string() + "/"
        + std::string(fixture.filename) + "." + std::string(target.name) + "."
        + std::string(target.extension);
    ASSERT_TRUE(WriteFile(output_path, result.body))
        << "Unable to write " << output_path;

    const ImagePtr decoded(vips_image_new_from_buffer(
        result.body.data(), result.body.size(), "", nullptr));
    ASSERT_NE(decoded, nullptr) << output_path << ": " << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()),
        static_cast<int>(fixture.expected[target_index].width));
    EXPECT_EQ(vips_image_get_height(decoded.get()),
        static_cast<int>(fixture.expected[target_index].height));
}

std::string ResizeParameterName(
    const testing::TestParamInfo<FixtureParameter>& parameter)
{
    const auto [fixture_index, target_index] = parameter.param;
    return "Fixture" + std::to_string(fixture_index) + "_"
        + std::string(targets[target_index].name);
}

INSTANTIATE_TEST_SUITE_P(AllFixtures, ResizeFixtureTest,
    testing::Combine(
        testing::Range<std::size_t>(0, fixtures.size()),
        testing::Range<std::size_t>(0, targets.size())),
    ResizeParameterName);

} // namespace
