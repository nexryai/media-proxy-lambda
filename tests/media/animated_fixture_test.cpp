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
    void operator()(VipsImage *image) const noexcept {
        if (image != nullptr) {
            g_object_unref(image);
        }
    }
};

using ImagePtr = std::unique_ptr<VipsImage, ImageUnref>;
using mediaproxy::http::parse_query;
using mediaproxy::http::PreferredOutput;
using mediaproxy::http::select_media_options;
using mediaproxy::media::convert_media;
using mediaproxy::media::EncodingQuality;
using mediaproxy::media::ImageDimensions;
using mediaproxy::media::initialize_vips;
using mediaproxy::media::MimeType;
using mediaproxy::media::OutputFormat;

struct Target {
    std::string_view name;
    std::string_view query;
    OutputFormat static_output;
};

constexpr std::array targets{
    Target{.name = "avatar", .query = "url=x&avatar=1", .static_output = OutputFormat::avif}, Target{.name = "emoji", .query = "url=x&emoji=1", .static_output = OutputFormat::avif},         Target{.name = "preview", .query = "url=x&preview=1", .static_output = OutputFormat::webp},
    Target{.name = "badge", .query = "url=x&badge=1", .static_output = OutputFormat::avif},   Target{.name = "thumbnail", .query = "url=x&thumbnail=1", .static_output = OutputFormat::webp}, Target{.name = "ticker", .query = "url=x&ticker=1", .static_output = OutputFormat::avif},
    Target{.name = "default", .query = "url=x", .static_output = OutputFormat::webp},
};

struct Fixture {
    std::string_view filename;
    MimeType mime;
    bool animated_output;
    std::array<int, targets.size()> page_counts;
    std::array<ImageDimensions, targets.size()> expected;
};

constexpr std::array static_dimensions_800_by_450{
    ImageDimensions{.width = 320, .height = 180}, ImageDimensions{.width = 228, .height = 128}, ImageDimensions{.width = 200, .height = 113}, ImageDimensions{.width = 96, .height = 54},
    ImageDimensions{.width = 500, .height = 281}, ImageDimensions{.width = 64, .height = 36},   ImageDimensions{.width = 800, .height = 450},
};

constexpr std::array animated_dimensions_800_by_450{
    ImageDimensions{.width = 320, .height = 180},
    ImageDimensions{.width = 228, .height = 128},
    // VIPS_SIZE_DOWN selects the height scale for this near-aspect target.
    ImageDimensions{.width = 201, .height = 113},
    ImageDimensions{.width = 96, .height = 54},
    ImageDimensions{.width = 500, .height = 281},
    ImageDimensions{.width = 64, .height = 36},
    ImageDimensions{.width = 800, .height = 450},
};

constexpr std::array fixtures{
    Fixture{.filename = "800x450_1.avif", .mime = MimeType::image_avif, .animated_output = true, .page_counts = {{325, 324, 322, 163, 325, 101, 325}}, .expected = static_dimensions_800_by_450},
    Fixture{.filename = "800x450_1.webp",
            .mime = MimeType::image_webp,
            .animated_output = true,
            // libwebp coalesces identical encoded frames at selector quality 65.
            .page_counts = {{234, 233, 229, 234, 234, 220, 234}},
            .expected = animated_dimensions_800_by_450},
    Fixture{.filename = "800x450_2.avif", .mime = MimeType::image_avif, .animated_output = true, .page_counts = {{99, 95, 95, 89, 100, 88, 100}}, .expected = static_dimensions_800_by_450},
    Fixture{.filename = "800x450_2.webp", .mime = MimeType::image_webp, .animated_output = true, .page_counts = {{325, 325, 325, 325, 325, 111, 325}}, .expected = animated_dimensions_800_by_450},
    Fixture{.filename = "animated-webp-supported.webp",
            .mime = MimeType::image_webp,
            .animated_output = true,
            .page_counts = {{12, 12, 12, 12, 12, 12, 12}},
            .expected = {{{.width = 320, .height = 320}, {.width = 400, .height = 400}, {.width = 200, .height = 200}, {.width = 96, .height = 96}, {.width = 400, .height = 400}, {.width = 64, .height = 64}, {.width = 400, .height = 400}}}},
    Fixture{.filename = "elephant.gif",
            .mime = MimeType::image_gif,
            .animated_output = true,
            .page_counts = {{34, 34, 34, 34, 34, 34, 34}},
            .expected = {{{.width = 320, .height = 267}, {.width = 480, .height = 400}, {.width = 200, .height = 167}, {.width = 96, .height = 80}, {.width = 480, .height = 400}, {.width = 64, .height = 53}, {.width = 480, .height = 400}}}},
    // The APNG compatibility path emits frame callback zero and ignores limits.
    Fixture{.filename = "elephant.png",
            .mime = MimeType::image_png,
            .animated_output = true,
            .page_counts = {{34, 34, 34, 34, 34, 34, 34}},
            .expected = {{{.width = 480, .height = 400}, {.width = 480, .height = 400}, {.width = 480, .height = 400}, {.width = 480, .height = 400}, {.width = 480, .height = 400}, {.width = 480, .height = 400}, {.width = 480, .height = 400}}}},
};

auto read_file(const std::filesystem::path &path) -> std::vector<std::byte> {
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
    input.read(reinterpret_cast<char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!input) {
        ADD_FAILURE() << "Unable to read " << path;
        return {};
    }

    return bytes;
}

auto write_file(const std::filesystem::path &path, std::span<const std::byte> bytes) -> bool {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }
    output.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));

    return static_cast<bool>(output);
}

auto to_output_format(PreferredOutput output) -> OutputFormat {

    return output == PreferredOutput::avif ? OutputFormat::avif : OutputFormat::webp;
}

auto extension(OutputFormat output) -> std::string_view {

    return output == OutputFormat::avif ? "avif" : "webp";
}

auto load_all(std::span<const std::byte> body) -> ImagePtr {
    ImagePtr loaded(vips_image_new_from_buffer(body.data(), body.size(), "", "n", -1, nullptr));
    if (loaded) {
        return loaded;
    }
    vips_error_clear();

    return ImagePtr(vips_image_new_from_buffer(body.data(), body.size(), "", nullptr));
}

auto metadata_int(VipsImage *image, const char *name, int fallback) -> int {
    int value = fallback;
    if (vips_image_get_typeof(image, name) != 0) {
        EXPECT_EQ(vips_image_get_int(image, name, &value), 0);
    }

    return value;
}

using FixtureParameter = std::tuple<std::size_t, std::size_t>;

class AnimatedFixtureTest : public testing::TestWithParam<FixtureParameter> {
  protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(initialize_vips()) << vips_error_buffer();
    }
};

TEST_P(AnimatedFixtureTest, WritesSelectorResultWithExpectedFrames) {
    const std::filesystem::path source_root{MEDIAPROXY_SOURCE_DIR};
    const auto fixture_directory = source_root / "tests/fixtures/media/animated";
    const auto result_directory = source_root / "tests/results/animated";
    std::error_code directory_error;
    std::filesystem::create_directories(result_directory, directory_error);
    ASSERT_FALSE(directory_error) << directory_error.message();

    const auto [fixture_index, target_index] = GetParam();
    const auto &fixture = fixtures[fixture_index];
    const auto &target = targets[target_index];
    SCOPED_TRACE(fixture.filename);
    SCOPED_TRACE(target.name);
    const auto input = read_file(fixture_directory / std::string(fixture.filename));
    ASSERT_FALSE(input.empty());

    const auto options = select_media_options(parse_query(target.query));
    const auto requested_output = to_output_format(options.preferred_output);
    EXPECT_EQ(requested_output, target.static_output);
    const auto expected_output = fixture.animated_output ? OutputFormat::webp : target.static_output;

    const auto result = convert_media(input, fixture.mime, options.force_static, requested_output, {.width = options.width_limit, .height = options.height_limit}, options.url_only ? EncodingQuality::url_only : EncodingQuality::standard);
    ASSERT_TRUE(result) << "Conversion failed with error " << static_cast<int>(result.error) << ": " << vips_error_buffer();
    EXPECT_EQ(result.encoded_format, expected_output);

    const auto output_path = result_directory / (std::string(fixture.filename) + "." + std::string(target.name) + "." + std::string(extension(expected_output)));
    ASSERT_TRUE(write_file(output_path, result.body)) << "Unable to write " << output_path;

    const ImagePtr decoded = load_all(result.body);
    ASSERT_NE(decoded, nullptr) << output_path << ": " << vips_error_buffer();
    const auto expected = fixture.expected[target_index];
    EXPECT_EQ(vips_image_get_width(decoded.get()), static_cast<int>(expected.width));
    const int page_count = metadata_int(decoded.get(), VIPS_META_N_PAGES, 1);
    const int page_height = metadata_int(decoded.get(), VIPS_META_PAGE_HEIGHT, vips_image_get_height(decoded.get()));
    const int expected_page_count = fixture.page_counts[target_index];
    EXPECT_EQ(page_count, expected_page_count);
    EXPECT_EQ(page_height, static_cast<int>(expected.height));
    ASSERT_LE(expected.height, static_cast<std::uint32_t>(std::numeric_limits<int>::max() / expected_page_count));
    EXPECT_EQ(vips_image_get_height(decoded.get()), static_cast<int>(expected.height) * expected_page_count);
}

auto animated_parameter_name(const testing::TestParamInfo<FixtureParameter> &parameter) -> std::string {
    const auto [fixture_index, target_index] = parameter.param;

    return "Fixture" + std::to_string(fixture_index) + "_" + std::string(targets[target_index].name);
}

INSTANTIATE_TEST_SUITE_P(AllFixtures, AnimatedFixtureTest, testing::Combine(testing::Range<std::size_t>(0, fixtures.size()), testing::Range<std::size_t>(0, targets.size())), animated_parameter_name);

} // namespace
