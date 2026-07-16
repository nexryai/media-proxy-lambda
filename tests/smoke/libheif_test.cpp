#include <string_view>

#include <gtest/gtest.h>
#include <libheif/heif.h>

TEST(BuildSmoke, ExposesOnlyPinnedLibheifAv1Codecs)
{
    EXPECT_EQ(heif_get_version_number_major(), 1);
    EXPECT_EQ(heif_get_version_number_minor(), 22);
    EXPECT_EQ(heif_get_version_number_maintenance(), 2);
    EXPECT_EQ(std::string_view{heif_get_version()}, "1.22.2");

    const heif_error init_error = heif_init(nullptr);
    ASSERT_EQ(init_error.code, heif_error_Ok) << init_error.message;

    EXPECT_NE(heif_have_decoder_for_format(heif_compression_AV1), 0);
    EXPECT_NE(heif_have_encoder_for_format(heif_compression_AV1), 0);
    EXPECT_EQ(heif_have_decoder_for_format(heif_compression_HEVC), 0);
    EXPECT_EQ(heif_have_encoder_for_format(heif_compression_HEVC), 0);

    heif_context* const context = heif_context_alloc();
    ASSERT_NE(context, nullptr);

    heif_encoder* av1_encoder = nullptr;
    const heif_error av1_error = heif_context_get_encoder_for_format(
        context, heif_compression_AV1, &av1_encoder);
    EXPECT_EQ(av1_error.code, heif_error_Ok) << av1_error.message;
    ASSERT_NE(av1_encoder, nullptr);
    heif_encoder_release(av1_encoder);

    heif_encoder* hevc_encoder = nullptr;
    const heif_error hevc_error = heif_context_get_encoder_for_format(
        context, heif_compression_HEVC, &hevc_encoder);
    EXPECT_NE(hevc_error.code, heif_error_Ok);
    EXPECT_EQ(hevc_encoder, nullptr);

    heif_context_free(context);
    heif_deinit();
}
