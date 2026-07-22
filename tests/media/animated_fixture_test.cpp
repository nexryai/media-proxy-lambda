#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
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
    OutputFormat static_output;
};

constexpr std::array targets{
    Target{"avatar", "avatar=1", OutputFormat::avif},
    Target{"emoji", "emoji=1", OutputFormat::avif},
    Target{"preview", "preview=1", OutputFormat::webp},
    Target{"badge", "badge=1", OutputFormat::avif},
    Target{"thumbnail", "thumbnail=1", OutputFormat::webp},
    Target{"ticker", "ticker=1", OutputFormat::avif},
    Target{"default", "", OutputFormat::webp},
};

struct Fixture {
    std::string_view filename;
    MimeType mime;
    bool animated_output;
    std::array<int, targets.size()> page_counts;
    std::array<ImageDimensions, targets.size()> expected;
};

constexpr std::array static_dimensions_800_by_450{
    ImageDimensions{320, 180},
    ImageDimensions{228, 128},
    ImageDimensions{200, 113},
    ImageDimensions{96, 54},
    ImageDimensions{500, 281},
    ImageDimensions{64, 36},
    ImageDimensions{800, 450},
};

constexpr std::array animated_dimensions_800_by_450{
    ImageDimensions{320, 180},
    ImageDimensions{228, 128},
    // VIPS_SIZE_DOWN selects the height scale for this near-aspect target.
    ImageDimensions{201, 113},
    ImageDimensions{96, 54},
    ImageDimensions{500, 281},
    ImageDimensions{64, 36},
    ImageDimensions{800, 450},
};

constexpr std::array fixtures{
    // AVIF sequences deliberately follow the specified static first-page path.
    Fixture{"800x450_1.avif", MimeType::image_avif, false,
        {{1, 1, 1, 1, 1, 1, 1}}, static_dimensions_800_by_450},
    Fixture{"800x450_1.webp", MimeType::image_webp, true,
        // At ticker size libwebp coalesces frames that become identical.
        {{234, 234, 234, 234, 234, 220, 234}},
        animated_dimensions_800_by_450},
    Fixture{"800x450_2.avif", MimeType::image_avif, false,
        {{1, 1, 1, 1, 1, 1, 1}}, static_dimensions_800_by_450},
    Fixture{"800x450_2.webp", MimeType::image_webp, true,
        {{325, 325, 325, 325, 325, 156, 325}},
        animated_dimensions_800_by_450},
    Fixture{"animated-webp-supported.webp", MimeType::image_webp, true,
        {{12, 12, 12, 12, 12, 12, 12}},
        {{{320, 320}, {400, 400}, {200, 200}, {96, 96}, {400, 400},
            {64, 64}, {400, 400}}}},
    Fixture{"elephant.gif", MimeType::image_gif, true,
        {{34, 34, 34, 34, 34, 34, 34}},
        {{{320, 267}, {480, 400}, {200, 167}, {96, 80}, {480, 400},
            {64, 53}, {480, 400}}}},
    // The APNG compatibility path omits frame callback zero and ignores limits.
    Fixture{"elephant.png", MimeType::image_png, true,
        {{33, 33, 33, 33, 33, 33, 33}},
        {{{480, 400}, {480, 400}, {480, 400}, {480, 400}, {480, 400},
            {480, 400}, {480, 400}}}},
};

std::vector<std::byte> ReadFile(const std::filesystem::path& path)
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
    const std::filesystem::path& path,
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

std::string_view Extension(OutputFormat output)
{
    return output == OutputFormat::avif ? "avif" : "webp";
}

ImagePtr LoadAll(std::span<const std::byte> body)
{
    ImagePtr loaded(vips_image_new_from_buffer(
        body.data(), body.size(), "", "n", -1, nullptr));
    if (loaded) {
        return loaded;
    }
    vips_error_clear();
    return ImagePtr(vips_image_new_from_buffer(
        body.data(), body.size(), "", nullptr));
}

int MetadataInt(VipsImage* image, const char* name, int fallback)
{
    int value = fallback;
    if (vips_image_get_typeof(image, name) != 0) {
        EXPECT_EQ(vips_image_get_int(image, name, &value), 0);
    }
    return value;
}

using FixtureParameter = std::tuple<std::size_t, std::size_t>;

class AnimatedFixtureTest : public testing::TestWithParam<FixtureParameter> {
protected:
    static void SetUpTestSuite()
    {
        ASSERT_TRUE(initialize_vips()) << vips_error_buffer();
    }
};

TEST_P(AnimatedFixtureTest, WritesSelectorResultWithExpectedFrames)
{
    const std::filesystem::path source_root{MEDIAPROXY_SOURCE_DIR};
    const auto fixture_directory =
        source_root / "tests/fixtures/media/animated";
    const auto result_directory = source_root / "tests/results/animated";
    std::error_code directory_error;
    std::filesystem::create_directories(result_directory, directory_error);
    ASSERT_FALSE(directory_error) << directory_error.message();

    const auto [fixture_index, target_index] = GetParam();
    const auto& fixture = fixtures[fixture_index];
    const auto& target = targets[target_index];
    SCOPED_TRACE(fixture.filename);
    SCOPED_TRACE(target.name);
    const auto input = ReadFile(
        fixture_directory / std::string(fixture.filename));
    ASSERT_FALSE(input.empty());

    const auto options = select_media_options(parse_query(target.query));
    const auto requested_output = ToOutputFormat(options.preferred_output);
    EXPECT_EQ(requested_output, target.static_output);
    const auto expected_output = fixture.animated_output
        ? OutputFormat::webp
        : target.static_output;

    const auto result = convert_media(input, fixture.mime,
        options.force_static, requested_output,
        {options.width_limit, options.height_limit});
    ASSERT_TRUE(result) << "Conversion failed with error "
                        << static_cast<int>(result.error) << ": "
                        << vips_error_buffer();
    EXPECT_EQ(result.encoded_format, expected_output);

    const auto output_path = result_directory
        / (std::string(fixture.filename) + "." + std::string(target.name)
            + "." + std::string(Extension(expected_output)));
    ASSERT_TRUE(WriteFile(output_path, result.body))
        << "Unable to write " << output_path;

    const ImagePtr decoded = LoadAll(result.body);
    ASSERT_NE(decoded, nullptr) << output_path << ": " << vips_error_buffer();
    const auto expected = fixture.expected[target_index];
    EXPECT_EQ(vips_image_get_width(decoded.get()),
        static_cast<int>(expected.width));
    const int page_count =
        MetadataInt(decoded.get(), VIPS_META_N_PAGES, 1);
    const int page_height = MetadataInt(decoded.get(),
        VIPS_META_PAGE_HEIGHT, vips_image_get_height(decoded.get()));
    const int expected_page_count = fixture.page_counts[target_index];
    EXPECT_EQ(page_count, expected_page_count);
    EXPECT_EQ(page_height, static_cast<int>(expected.height));
    ASSERT_LE(expected.height,
        static_cast<std::uint32_t>(
            std::numeric_limits<int>::max() / expected_page_count));
    EXPECT_EQ(vips_image_get_height(decoded.get()),
        static_cast<int>(expected.height) * expected_page_count);
}

std::string AnimatedParameterName(
    const testing::TestParamInfo<FixtureParameter>& parameter)
{
    const auto [fixture_index, target_index] = parameter.param;
    return "Fixture" + std::to_string(fixture_index) + "_"
        + std::string(targets[target_index].name);
}

INSTANTIATE_TEST_SUITE_P(AllFixtures, AnimatedFixtureTest,
    testing::Combine(
        testing::Range<std::size_t>(0, fixtures.size()),
        testing::Range<std::size_t>(0, targets.size())),
    AnimatedParameterName);

} // namespace
