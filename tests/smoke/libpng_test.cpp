#include <array>

#include <gtest/gtest.h>
#include <png.h>

namespace {

class PngImage final {
public:
    PngImage() noexcept
    {
        image_.version = PNG_IMAGE_VERSION;
    }

    ~PngImage()
    {
        png_image_free(&image_);
    }

    PngImage(const PngImage&) = delete;
    PngImage& operator=(const PngImage&) = delete;

    [[nodiscard]] png_image* get() noexcept
    {
        return &image_;
    }

private:
    png_image image_{};
};

} // namespace

TEST(BuildSmoke, RoundTripsWithPinnedLibPng)
{
    constexpr std::array<png_byte, 4> expected_pixel = {17, 34, 51, 68};
    PngImage writer;
    writer.get()->width = 1;
    writer.get()->height = 1;
    writer.get()->format = PNG_FORMAT_RGBA;

    std::array<png_byte, 128> encoded{};
    png_alloc_size_t encoded_size = encoded.size();
    ASSERT_NE(
        png_image_write_to_memory(
            writer.get(),
            encoded.data(),
            &encoded_size,
            0,
            expected_pixel.data(),
            0,
            nullptr),
        0);
    ASSERT_GT(encoded_size, 8U);
    ASSERT_LE(encoded_size, encoded.size());

    PngImage reader;
    ASSERT_NE(
        png_image_begin_read_from_memory(
            reader.get(), encoded.data(), encoded_size),
        0);
    EXPECT_EQ(reader.get()->width, 1U);
    EXPECT_EQ(reader.get()->height, 1U);
    reader.get()->format = PNG_FORMAT_RGBA;
    std::array<png_byte, 4> decoded_pixel{};
    ASSERT_NE(
        png_image_finish_read(
            reader.get(), nullptr, decoded_pixel.data(), 0, nullptr),
        0);
    EXPECT_EQ(decoded_pixel, expected_pixel);

    PngImage truncated;
    if (png_image_begin_read_from_memory(
            truncated.get(), encoded.data(), encoded_size / 2)
        != 0) {
        truncated.get()->format = PNG_FORMAT_RGBA;
        std::array<png_byte, 4> discarded{};
        EXPECT_EQ(
            png_image_finish_read(
                truncated.get(), nullptr, discarded.data(), 0, nullptr),
            0);
    }
}
