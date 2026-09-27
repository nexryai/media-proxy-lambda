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
    std::string_view extension;
    OutputFormat output;
};

constexpr std::array targets{
    Target{.name = "avatar", .query = "url=x&avatar=1", .extension = "avif", .output = OutputFormat::avif},
    Target{.name = "emoji", .query = "url=x&emoji=1", .extension = "avif", .output = OutputFormat::avif},
    Target{.name = "preview", .query = "url=x&preview=1", .extension = "webp", .output = OutputFormat::webp},
    Target{.name = "badge", .query = "url=x&badge=1", .extension = "avif", .output = OutputFormat::avif},
    Target{.name = "thumbnail", .query = "url=x&thumbnail=1", .extension = "webp", .output = OutputFormat::webp},
    Target{.name = "ticker", .query = "url=x&ticker=1", .extension = "avif", .output = OutputFormat::avif},
    Target{.name = "default", .query = "url=x", .extension = "webp", .output = OutputFormat::webp},
};

struct Fixture {
    std::string_view filename;
    MimeType mime;
    std::array<ImageDimensions, targets.size()> expected;
};

constexpr std::array fixtures{
    Fixture{.filename = "1500 x749.jpg",
            .mime = MimeType::image_jpeg,
            .expected = {{{.width = 320, .height = 160}, {.width = 700, .height = 350}, {.width = 200, .height = 100}, {.width = 96, .height = 48}, {.width = 500, .height = 250}, {.width = 64, .height = 32}, {.width = 1500, .height = 749}}}},
    Fixture{.filename = "1500x843.jpg", .mime = MimeType::image_jpeg, .expected = {{{.width = 320, .height = 180}, {.width = 700, .height = 393}, {.width = 200, .height = 112}, {.width = 96, .height = 54}, {.width = 500, .height = 281}, {.width = 64, .height = 36}, {.width = 1500, .height = 843}}}},
    Fixture{.filename = "602 x602.jpg", .mime = MimeType::image_jpeg, .expected = {{{.width = 320, .height = 320}, {.width = 128, .height = 128}, {.width = 200, .height = 200}, {.width = 96, .height = 96}, {.width = 400, .height = 400}, {.width = 64, .height = 64}, {.width = 602, .height = 602}}}},
    Fixture{.filename = "7680x4320_1.png",
            .mime = MimeType::image_png,
            .expected = {{{.width = 320, .height = 180}, {.width = 700, .height = 394}, {.width = 200, .height = 113}, {.width = 96, .height = 54}, {.width = 500, .height = 281}, {.width = 64, .height = 36}, {.width = 3200, .height = 1800}}}},
    Fixture{.filename = "7680x4320_1.avif",
            .mime = MimeType::image_avif,
            .expected = {{{.width = 320, .height = 180}, {.width = 700, .height = 394}, {.width = 200, .height = 113}, {.width = 96, .height = 54}, {.width = 500, .height = 281}, {.width = 64, .height = 36}, {.width = 3200, .height = 1800}}}},
    Fixture{.filename = "7680x4320_1.webp",
            .mime = MimeType::image_webp,
            .expected = {{{.width = 320, .height = 180}, {.width = 700, .height = 394}, {.width = 200, .height = 113}, {.width = 96, .height = 54}, {.width = 500, .height = 281}, {.width = 64, .height = 36}, {.width = 3200, .height = 1800}}}},
    Fixture{.filename = "7680x4320_lossless.avif",
            .mime = MimeType::image_avif,
            .expected = {{{.width = 320, .height = 180}, {.width = 700, .height = 394}, {.width = 200, .height = 113}, {.width = 96, .height = 54}, {.width = 500, .height = 281}, {.width = 64, .height = 36}, {.width = 3200, .height = 1800}}}},
    Fixture{.filename = "7680x4320_lossless.webp",
            .mime = MimeType::image_webp,
            .expected = {{{.width = 320, .height = 180}, {.width = 700, .height = 394}, {.width = 200, .height = 113}, {.width = 96, .height = 54}, {.width = 500, .height = 281}, {.width = 64, .height = 36}, {.width = 3200, .height = 1800}}}},
};

auto read_file(const std::string &path) -> std::vector<std::byte> {
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

auto write_file(const std::string &path, std::span<const std::byte> bytes) -> bool {
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

using FixtureParameter = std::tuple<std::size_t, std::size_t>;

class ResizeFixtureTest : public testing::TestWithParam<FixtureParameter> {
  protected:
    static void SetUpTestSuite() {
        ASSERT_TRUE(initialize_vips()) << vips_error_buffer();
    }
};

TEST_P(ResizeFixtureTest, WritesSelectorResultAtExpectedDimensions) {
    const std::filesystem::path source_root{MEDIAPROXY_SOURCE_DIR};
    const auto result_directory = source_root / "tests/results/resize";
    std::error_code directory_error;
    std::filesystem::create_directories(result_directory, directory_error);
    ASSERT_FALSE(directory_error) << directory_error.message();

    const auto [fixture_index, target_index] = GetParam();
    const auto &fixture = fixtures[fixture_index];
    const auto &target = targets[target_index];
    SCOPED_TRACE(fixture.filename);
    SCOPED_TRACE(target.name);
    const std::string fixture_path = std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/fixtures/media/resize/" + std::string(fixture.filename);
    const auto input = read_file(fixture_path);
    ASSERT_FALSE(input.empty());

    const auto options = select_media_options(parse_query(target.query));
    const auto requested_output = to_output_format(options.preferred_output);
    EXPECT_EQ(requested_output, target.output);

    const auto result = convert_media(input, fixture.mime, options.force_static, requested_output, {.width = options.width_limit, .height = options.height_limit}, options.url_only ? EncodingQuality::url_only : EncodingQuality::standard);
    ASSERT_TRUE(result) << "Conversion failed with error " << static_cast<int>(result.error) << ": " << vips_error_buffer();
    EXPECT_EQ(result.encoded_format, target.output);

    const std::string output_path = result_directory.string() + "/" + std::string(fixture.filename) + "." + std::string(target.name) + "." + std::string(target.extension);
    ASSERT_TRUE(write_file(output_path, result.body)) << "Unable to write " << output_path;

    const ImagePtr decoded(vips_image_new_from_buffer(result.body.data(), result.body.size(), "", nullptr));
    ASSERT_NE(decoded, nullptr) << output_path << ": " << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), static_cast<int>(fixture.expected[target_index].width));
    EXPECT_EQ(vips_image_get_height(decoded.get()), static_cast<int>(fixture.expected[target_index].height));
}

auto resize_parameter_name(const testing::TestParamInfo<FixtureParameter> &parameter) -> std::string {
    const auto [fixture_index, target_index] = parameter.param;

    return "Fixture" + std::to_string(fixture_index) + "_" + std::string(targets[target_index].name);
}

INSTANTIATE_TEST_SUITE_P(AllFixtures, ResizeFixtureTest, testing::Combine(testing::Range<std::size_t>(0, fixtures.size()), testing::Range<std::size_t>(0, targets.size())), resize_parameter_name);

} // namespace
