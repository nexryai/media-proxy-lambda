#include <string_view>

#include <aom/aom_codec.h>
#include <aom/aom_decoder.h>
#include <aom/aom_encoder.h>
#include <aom/aomcx.h>
#include <aom/aomdx.h>
#include <gtest/gtest.h>

TEST(BuildSmoke, InitializesPinnedLibaomAv1Codecs)
{
    EXPECT_EQ(aom_codec_version_major(), 3);
    EXPECT_EQ(aom_codec_version_minor(), 14);
    EXPECT_EQ(aom_codec_version_patch(), 1);
    EXPECT_EQ(std::string_view{aom_codec_version_str()}, "v3.14.1");

    aom_codec_iface_t* const encoder_interface = aom_codec_av1_cx();
    aom_codec_iface_t* const decoder_interface = aom_codec_av1_dx();
    ASSERT_NE(encoder_interface, nullptr);
    ASSERT_NE(decoder_interface, nullptr);

    aom_codec_enc_cfg_t encoder_config{};
    ASSERT_EQ(aom_codec_enc_config_default(encoder_interface,
                  &encoder_config, AOM_USAGE_ALL_INTRA),
        AOM_CODEC_OK);
    encoder_config.g_w = 16;
    encoder_config.g_h = 16;
    encoder_config.g_threads = 1;
    encoder_config.g_lag_in_frames = 0;

    aom_codec_ctx_t encoder{};
    ASSERT_EQ(aom_codec_enc_init(&encoder, encoder_interface,
                  &encoder_config, 0),
        AOM_CODEC_OK);
    EXPECT_EQ(aom_codec_destroy(&encoder), AOM_CODEC_OK);

    aom_codec_ctx_t decoder{};
    ASSERT_EQ(aom_codec_dec_init(&decoder, decoder_interface, nullptr, 0),
        AOM_CODEC_OK);
    EXPECT_EQ(aom_codec_destroy(&decoder), AOM_CODEC_OK);
}
