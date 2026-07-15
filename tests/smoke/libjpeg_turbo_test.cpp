#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <memory>

#include <gtest/gtest.h>
#include <jpeglib.h>

namespace {

class JpegCompressor final {
public:
    JpegCompressor() noexcept
    {
        state_.err = jpeg_std_error(&errors_);
        jpeg_create_compress(&state_);
    }

    ~JpegCompressor()
    {
        jpeg_destroy_compress(&state_);
    }

    JpegCompressor(const JpegCompressor&) = delete;
    JpegCompressor& operator=(const JpegCompressor&) = delete;

    [[nodiscard]] jpeg_compress_struct* get() noexcept
    {
        return &state_;
    }

private:
    jpeg_compress_struct state_{};
    jpeg_error_mgr errors_{};
};

class JpegDecompressor final {
public:
    JpegDecompressor() noexcept
    {
        state_.err = jpeg_std_error(&errors_);
        jpeg_create_decompress(&state_);
    }

    ~JpegDecompressor()
    {
        jpeg_destroy_decompress(&state_);
    }

    JpegDecompressor(const JpegDecompressor&) = delete;
    JpegDecompressor& operator=(const JpegDecompressor&) = delete;

    [[nodiscard]] jpeg_decompress_struct* get() noexcept
    {
        return &state_;
    }

private:
    jpeg_decompress_struct state_{};
    jpeg_error_mgr errors_{};
};

} // namespace

TEST(BuildSmoke, RoundTripsWithPinnedLibJpegTurbo)
{
    constexpr std::array<JSAMPLE, 3> expected_pixel = {17, 101, 203};
    JpegCompressor compressor;
    compressor.get()->mem->max_memory_to_use = 1024L * 1024L;
    unsigned char* encoded_bytes = nullptr;
    unsigned long encoded_size = 0;
    jpeg_mem_dest(compressor.get(), &encoded_bytes, &encoded_size);
    compressor.get()->image_width = 1;
    compressor.get()->image_height = 1;
    compressor.get()->input_components = 3;
    compressor.get()->in_color_space = JCS_RGB;
    jpeg_set_defaults(compressor.get());
    for (int index = 0; index < compressor.get()->num_components; ++index) {
        compressor.get()->comp_info[index].h_samp_factor = 1;
        compressor.get()->comp_info[index].v_samp_factor = 1;
    }
    jpeg_set_quality(compressor.get(), 100, TRUE);
    jpeg_start_compress(compressor.get(), TRUE);
    JSAMPROW input_row = const_cast<JSAMPLE*>(expected_pixel.data());
    ASSERT_EQ(jpeg_write_scanlines(compressor.get(), &input_row, 1), 1U);
    jpeg_finish_compress(compressor.get());
    std::unique_ptr<unsigned char, decltype(&std::free)> encoded(
        encoded_bytes, &std::free);
    ASSERT_NE(encoded, nullptr);
    ASSERT_GT(encoded_size, 4U);
    EXPECT_EQ(encoded.get()[0], 0xffU);
    EXPECT_EQ(encoded.get()[1], 0xd8U);
    EXPECT_EQ(encoded.get()[encoded_size - 2], 0xffU);
    EXPECT_EQ(encoded.get()[encoded_size - 1], 0xd9U);

    JpegDecompressor decompressor;
    decompressor.get()->mem->max_memory_to_use = 1024L * 1024L;
    jpeg_mem_src(decompressor.get(), encoded.get(), encoded_size);
    ASSERT_EQ(jpeg_read_header(decompressor.get(), TRUE), JPEG_HEADER_OK);
    decompressor.get()->out_color_space = JCS_RGB;
    ASSERT_NE(jpeg_start_decompress(decompressor.get()), FALSE);
    ASSERT_EQ(decompressor.get()->output_width, 1U);
    ASSERT_EQ(decompressor.get()->output_height, 1U);
    ASSERT_EQ(decompressor.get()->output_components, 3);
    std::array<JSAMPLE, 3> decoded_pixel{};
    JSAMPROW output_row = decoded_pixel.data();
    ASSERT_EQ(jpeg_read_scanlines(decompressor.get(), &output_row, 1), 1U);
    ASSERT_NE(jpeg_finish_decompress(decompressor.get()), FALSE);
    for (std::size_t index = 0; index < expected_pixel.size(); ++index) {
        EXPECT_NEAR(decoded_pixel[index], expected_pixel[index], 2);
    }
}
