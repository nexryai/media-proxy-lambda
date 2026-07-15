#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <memory>

#include <gtest/gtest.h>

extern "C" {
#include <nsgif.h>
}

namespace {

struct Bitmap {
    std::size_t pixel_bytes;
};

static_assert(sizeof(Bitmap) % alignof(std::uint32_t) == 0);

nsgif_bitmap_t* CreateBitmap(int width, int height) noexcept
{
    if (width <= 0 || height <= 0) {
        return nullptr;
    }
    constexpr std::size_t bytes_per_pixel = 4;
    const auto pixel_width = static_cast<std::size_t>(width);
    const auto pixel_height = static_cast<std::size_t>(height);
    if (pixel_width > std::numeric_limits<std::size_t>::max() / pixel_height
            / bytes_per_pixel) {
        return nullptr;
    }
    const std::size_t pixel_bytes =
        pixel_width * pixel_height * bytes_per_pixel;
    if (pixel_bytes > std::numeric_limits<std::size_t>::max()
            - sizeof(Bitmap)) {
        return nullptr;
    }
    auto* const bitmap = static_cast<Bitmap*>(
        std::calloc(1, sizeof(Bitmap) + pixel_bytes));
    if (bitmap != nullptr) {
        bitmap->pixel_bytes = pixel_bytes;
    }
    return bitmap;
}

void DestroyBitmap(nsgif_bitmap_t* bitmap) noexcept
{
    std::free(bitmap);
}

std::uint8_t* GetBitmapBuffer(nsgif_bitmap_t* bitmap) noexcept
{
    return reinterpret_cast<std::uint8_t*>(
        static_cast<Bitmap*>(bitmap) + 1);
}

constexpr nsgif_bitmap_cb_vt bitmap_callbacks = {
    .create = &CreateBitmap,
    .destroy = &DestroyBitmap,
    .get_buffer = &GetBitmapBuffer,
    .set_opaque = nullptr,
    .test_opaque = nullptr,
    .modified = nullptr,
    .get_rowspan = nullptr,
};

} // namespace

TEST(BuildSmoke, DecodesPinnedLibNsgifInMemory)
{
    constexpr std::array<std::uint8_t, 35> encoded = {
        'G', 'I', 'F', '8', '9', 'a',
        0x01, 0x00, 0x01, 0x00, 0x80, 0x00, 0x00,
        0x00, 0x00, 0x00, 0xff, 0xff, 0xff,
        0x2c, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00,
        0x02, 0x02, 0x44, 0x01, 0x00, 0x3b,
    };

    nsgif_t* raw_gif = nullptr;
    ASSERT_EQ(
        nsgif_create(
            &bitmap_callbacks, NSGIF_BITMAP_FMT_R8G8B8A8, &raw_gif),
        NSGIF_OK);
    std::unique_ptr<nsgif_t, decltype(&nsgif_destroy)> gif(
        raw_gif, &nsgif_destroy);
    ASSERT_NE(gif, nullptr);
    ASSERT_EQ(nsgif_data_scan(gif.get(), encoded.size(), encoded.data()),
        NSGIF_OK);
    nsgif_data_complete(gif.get());

    const nsgif_info_t* const info = nsgif_get_info(gif.get());
    ASSERT_NE(info, nullptr);
    EXPECT_EQ(info->width, 1U);
    EXPECT_EQ(info->height, 1U);
    EXPECT_EQ(info->frame_count, 1U);

    nsgif_rect_t area{};
    std::uint32_t delay = 0;
    std::uint32_t frame = 1;
    ASSERT_EQ(
        nsgif_frame_prepare(gif.get(), &area, &delay, &frame), NSGIF_OK);
    EXPECT_EQ(frame, 0U);
    EXPECT_EQ(delay, NSGIF_INFINITE);
    EXPECT_EQ(area.x0, 0U);
    EXPECT_EQ(area.y0, 0U);
    EXPECT_EQ(area.x1, 1U);
    EXPECT_EQ(area.y1, 1U);

    nsgif_bitmap_t* decoded = nullptr;
    ASSERT_EQ(nsgif_frame_decode(gif.get(), frame, &decoded), NSGIF_OK);
    ASSERT_NE(decoded, nullptr);
    const auto* const pixels = GetBitmapBuffer(decoded);
    EXPECT_EQ(pixels[0], 0U);
    EXPECT_EQ(pixels[1], 0U);
    EXPECT_EQ(pixels[2], 0U);
    EXPECT_EQ(pixels[3], 0xffU);
}
