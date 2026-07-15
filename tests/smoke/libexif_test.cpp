#include <cstddef>
#include <cstdlib>
#include <memory>

#include <gtest/gtest.h>
#include <libexif/exif-data.h>
#include <libexif/exif-entry.h>
#include <libexif/exif-tag.h>
#include <libexif/exif-utils.h>

namespace {

struct FreeDeleter {
    void operator()(unsigned char* data) const noexcept
    {
        std::free(data);
    }
};

using ExifDataPtr =
    std::unique_ptr<ExifData, decltype(&exif_data_unref)>;
using ExifEntryPtr =
    std::unique_ptr<ExifEntry, decltype(&exif_entry_unref)>;

} // namespace

TEST(BuildSmoke, RoundTripsPinnedLibExifOrientationInMemory)
{
    ExifDataPtr generated(exif_data_new(), &exif_data_unref);
    ASSERT_NE(generated, nullptr);
    exif_data_set_byte_order(generated.get(), EXIF_BYTE_ORDER_INTEL);

    ExifEntryPtr orientation(exif_entry_new(), &exif_entry_unref);
    ASSERT_NE(orientation, nullptr);
    exif_content_add_entry(
        generated->ifd[EXIF_IFD_0], orientation.get());
    exif_entry_initialize(orientation.get(), EXIF_TAG_ORIENTATION);
    ASSERT_NE(orientation->data, nullptr);
    ASSERT_GE(orientation->size, sizeof(ExifShort));
    constexpr ExifShort right_top_orientation = 6;
    exif_set_short(orientation->data, EXIF_BYTE_ORDER_INTEL,
        right_top_orientation);

    unsigned char* raw_encoded = nullptr;
    unsigned int encoded_size = 0;
    exif_data_save_data(generated.get(), &raw_encoded, &encoded_size);
    std::unique_ptr<unsigned char, FreeDeleter> encoded(raw_encoded);
    ASSERT_NE(encoded, nullptr);
    ASSERT_GT(encoded_size, 0U);
    constexpr unsigned int maximum_exif_size = 64U * 1024U;
    ASSERT_LE(encoded_size, maximum_exif_size);

    ExifDataPtr parsed(
        exif_data_new_from_data(encoded.get(), encoded_size),
        &exif_data_unref);
    ASSERT_NE(parsed, nullptr);
    ExifEntry* const parsed_orientation = exif_content_get_entry(
        parsed->ifd[EXIF_IFD_0], EXIF_TAG_ORIENTATION);
    ASSERT_NE(parsed_orientation, nullptr);
    ASSERT_NE(parsed_orientation->data, nullptr);
    ASSERT_GE(parsed_orientation->size, sizeof(ExifShort));
    EXPECT_EQ(exif_get_short(parsed_orientation->data,
                  exif_data_get_byte_order(parsed.get())),
        right_top_orientation);
}
