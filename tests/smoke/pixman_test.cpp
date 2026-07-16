#include <array>
#include <cstdint>

#include <gtest/gtest.h>
#include <pixman.h>

TEST(PixmanBuild, CompositesBoundedArgbPixels)
{
    std::array<std::uint32_t, 1> source_pixels = {0x80800000U};
    std::array<std::uint32_t, 1> destination_pixels = {0xff0000ffU};

    pixman_image_t* const source = pixman_image_create_bits(
        PIXMAN_a8r8g8b8, 1, 1, source_pixels.data(), sizeof(std::uint32_t));
    ASSERT_NE(source, nullptr);
    pixman_image_t* const destination = pixman_image_create_bits(
        PIXMAN_a8r8g8b8, 1, 1, destination_pixels.data(),
        sizeof(std::uint32_t));
    ASSERT_NE(destination, nullptr);

    pixman_image_composite32(
        PIXMAN_OP_OVER, source, nullptr, destination, 0, 0, 0, 0, 0, 0, 1, 1);
    EXPECT_EQ(destination_pixels[0], 0xff80007fU);
    EXPECT_EQ(pixman_version(), PIXMAN_VERSION);
    EXPECT_STREQ(pixman_version_string(), PIXMAN_VERSION_STRING);

    EXPECT_TRUE(pixman_image_unref(destination));
    EXPECT_TRUE(pixman_image_unref(source));
}
