#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>

#include <gtest/gtest.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>
#include <webp/mux.h>
#include <webp/sharpyuv/sharpyuv.h>

namespace {

class WebPDataOwner final {
public:
    WebPDataOwner() noexcept
    {
        WebPDataInit(&data_);
    }

    ~WebPDataOwner()
    {
        WebPDataClear(&data_);
    }

    WebPDataOwner(const WebPDataOwner&) = delete;
    WebPDataOwner& operator=(const WebPDataOwner&) = delete;

    [[nodiscard]] WebPData* get() noexcept
    {
        return &data_;
    }

private:
    WebPData data_{};
};

} // namespace

TEST(BuildSmoke, AssemblesPinnedAnimatedWebPInMemory)
{
    constexpr int required_webp_version = 0x010600;
    EXPECT_EQ(WebPGetDecoderVersion(), required_webp_version);
    EXPECT_EQ(WebPGetEncoderVersion(), required_webp_version);
    EXPECT_EQ(WebPGetDemuxVersion(), required_webp_version);
    EXPECT_EQ(WebPGetMuxVersion(), required_webp_version);
    EXPECT_EQ(SharpYuvGetVersion(), SHARPYUV_VERSION);

    constexpr std::array<std::uint8_t, 4> first_pixel = {17, 34, 51, 68};
    constexpr std::array<std::uint8_t, 4> second_pixel = {85, 102, 119, 136};
    std::uint8_t* first_bytes = nullptr;
    const std::size_t first_size = WebPEncodeLosslessRGBA(
        first_pixel.data(), 1, 1, 4, &first_bytes);
    std::unique_ptr<std::uint8_t, decltype(&WebPFree)> first(
        first_bytes, &WebPFree);
    ASSERT_GT(first_size, 0U);
    ASSERT_NE(first, nullptr);

    std::uint8_t* second_bytes = nullptr;
    const std::size_t second_size = WebPEncodeLosslessRGBA(
        second_pixel.data(), 1, 1, 4, &second_bytes);
    std::unique_ptr<std::uint8_t, decltype(&WebPFree)> second(
        second_bytes, &WebPFree);
    ASSERT_GT(second_size, 0U);
    ASSERT_NE(second, nullptr);

    int width = 0;
    int height = 0;
    EXPECT_NE(WebPGetInfo(first.get(), first_size, &width, &height), 0);
    EXPECT_EQ(width, 1);
    EXPECT_EQ(height, 1);
    EXPECT_EQ(WebPGetInfo(first.get(), first_size / 2, nullptr, nullptr), 0);
    int decoded_width = 0;
    int decoded_height = 0;
    std::unique_ptr<std::uint8_t, decltype(&WebPFree)> decoded(
        WebPDecodeRGBA(
            first.get(), first_size, &decoded_width, &decoded_height),
        &WebPFree);
    ASSERT_NE(decoded, nullptr);
    ASSERT_EQ(decoded_width, 1);
    ASSERT_EQ(decoded_height, 1);
    for (std::size_t index = 0; index < first_pixel.size(); ++index) {
        EXPECT_EQ(decoded.get()[index], first_pixel[index]);
    }

    std::unique_ptr<WebPMux, decltype(&WebPMuxDelete)> mux(
        WebPMuxNew(), &WebPMuxDelete);
    ASSERT_NE(mux, nullptr);
    ASSERT_EQ(WebPMuxSetCanvasSize(mux.get(), 1, 1), WEBP_MUX_OK);
    constexpr WebPMuxAnimParams animation = {
        .bgcolor = 0,
        .loop_count = 3,
    };
    ASSERT_EQ(
        WebPMuxSetAnimationParams(mux.get(), &animation), WEBP_MUX_OK);

    WebPMuxFrameInfo frame{};
    frame.bitstream = {.bytes = first.get(), .size = first_size};
    frame.duration = 40;
    frame.id = WEBP_CHUNK_ANMF;
    frame.dispose_method = WEBP_MUX_DISPOSE_NONE;
    frame.blend_method = WEBP_MUX_NO_BLEND;
    ASSERT_EQ(WebPMuxPushFrame(mux.get(), &frame, 1), WEBP_MUX_OK);
    frame.bitstream = {.bytes = second.get(), .size = second_size};
    frame.duration = 60;
    frame.blend_method = WEBP_MUX_BLEND;
    ASSERT_EQ(WebPMuxPushFrame(mux.get(), &frame, 1), WEBP_MUX_OK);

    WebPDataOwner assembled;
    ASSERT_EQ(WebPMuxAssemble(mux.get(), assembled.get()), WEBP_MUX_OK);
    ASSERT_NE(assembled.get()->bytes, nullptr);
    ASSERT_GT(assembled.get()->size, 0U);

    std::unique_ptr<WebPDemuxer, decltype(&WebPDemuxDelete)> demux(
        WebPDemux(assembled.get()), &WebPDemuxDelete);
    ASSERT_NE(demux, nullptr);
    EXPECT_EQ(WebPDemuxGetI(demux.get(), WEBP_FF_CANVAS_WIDTH), 1U);
    EXPECT_EQ(WebPDemuxGetI(demux.get(), WEBP_FF_CANVAS_HEIGHT), 1U);
    EXPECT_EQ(WebPDemuxGetI(demux.get(), WEBP_FF_FRAME_COUNT), 2U);
    EXPECT_EQ(WebPDemuxGetI(demux.get(), WEBP_FF_LOOP_COUNT), 3U);

    WebPIterator raw_iterator{};
    ASSERT_NE(WebPDemuxGetFrame(demux.get(), 1, &raw_iterator), 0);
    std::unique_ptr<WebPIterator, decltype(&WebPDemuxReleaseIterator)>
        iterator(&raw_iterator, &WebPDemuxReleaseIterator);
    EXPECT_EQ(iterator->duration, 40);
    EXPECT_EQ(iterator->dispose_method, WEBP_MUX_DISPOSE_NONE);
    EXPECT_EQ(iterator->blend_method, WEBP_MUX_NO_BLEND);
    ASSERT_NE(WebPDemuxNextFrame(iterator.get()), 0);
    EXPECT_EQ(iterator->duration, 60);
    EXPECT_EQ(iterator->blend_method, WEBP_MUX_BLEND);
}
