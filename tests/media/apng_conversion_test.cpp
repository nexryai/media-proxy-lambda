#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include <glib.h>
#include <gtest/gtest.h>
#include <mediaproxy/media/apng_conversion.hpp>
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
using mediaproxy::media::convert_apng_to_webp;
using mediaproxy::media::initialize_vips;

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

TEST_F(ApngConversionTest, OmitsBaseCallbackAndUsesTargetDimensions)
{
    const auto result =
        convert_apng_to_webp(ReadFixture("over-none.png"), 8, 6);
    ASSERT_TRUE(result) << static_cast<int>(result.error);
    ImagePtr decoded(vips_image_new_from_buffer(result.body.data(),
        result.body.size(), "", "n", -1, nullptr));
    ASSERT_NE(decoded, nullptr) << vips_error_buffer();
    EXPECT_EQ(vips_image_get_width(decoded.get()), 8);
    EXPECT_EQ(vips_image_get_height(decoded.get()), 12);
    int pages = 0;
    int page_height = 0;
    ASSERT_EQ(vips_image_get_int(decoded.get(), VIPS_META_N_PAGES, &pages), 0);
    ASSERT_EQ(vips_image_get_int(
                  decoded.get(), VIPS_META_PAGE_HEIGHT, &page_height),
        0);
    EXPECT_EQ(pages, 2);
    EXPECT_EQ(page_height, 6);
}

TEST_F(ApngConversionTest, RejectsMalformedAndZeroTarget)
{
    EXPECT_FALSE(convert_apng_to_webp(
        ReadFixture("invalid-crc.png"), 4, 4));
    EXPECT_FALSE(convert_apng_to_webp(
        ReadFixture("over-none.png"), 0, 4));
}

} // namespace
