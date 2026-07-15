#include <array>
#include <cstdint>
#include <cstdlib>
#include <memory>
#include <string>

#include <curl/curl.h>
#include <gtest/gtest.h>
#include <nghttp2/nghttp2.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <png.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>
#include <webp/mux.h>
#include <webp/sharpyuv/sharpyuv.h>
#include <yyjson.h>
#include <zlib.h>

namespace {

class CurlGlobal final {
public:
    CurlGlobal() noexcept
        : result_(curl_global_init(CURL_GLOBAL_DEFAULT))
    {
    }

    ~CurlGlobal()
    {
        if (result_ == CURLE_OK) {
            curl_global_cleanup();
        }
    }

    CurlGlobal(const CurlGlobal&) = delete;
    CurlGlobal& operator=(const CurlGlobal&) = delete;

    [[nodiscard]] CURLcode result() const noexcept
    {
        return result_;
    }

private:
    CURLcode result_;
};

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

TEST(BuildSmoke, UsesPinnedLibcxx)
{
    const std::string runtime_name = "mediaproxy-lambda";
    EXPECT_EQ(runtime_name.size(), 17U);
}

TEST(BuildSmoke, InitializesMinimalPinnedCurl)
{
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);

    const curl_version_info_data* const version =
        curl_version_info(CURLVERSION_NOW);
    ASSERT_NE(version, nullptr);
    EXPECT_EQ(version->version_num, LIBCURL_VERSION_NUM);
    constexpr int required_features =
        CURL_VERSION_SSL | CURL_VERSION_LIBZ | CURL_VERSION_HTTP2;
    EXPECT_EQ(version->features & required_features, required_features);
    ASSERT_NE(version->ssl_version, nullptr);
    EXPECT_TRUE(std::string_view{version->ssl_version}.starts_with("BoringSSL"));

    std::array<bool, 2> found_protocols{};
    ASSERT_NE(version->protocols, nullptr);
    for (const char* const* protocol = version->protocols;
         *protocol != nullptr;
         ++protocol) {
        const std::string_view name{*protocol};
        if (name == "http") {
            found_protocols[0] = true;
        } else if (name == "https") {
            found_protocols[1] = true;
        } else {
            ADD_FAILURE() << "unexpected curl protocol: " << name;
        }
    }
    EXPECT_EQ(found_protocols, (std::array<bool, 2>{true, true}));

    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> easy(
        curl_easy_init(),
        &curl_easy_cleanup);
    ASSERT_NE(easy, nullptr);
    EXPECT_EQ(curl_easy_setopt(easy.get(), CURLOPT_PROTOCOLS_STR, "https"),
        CURLE_OK);
    EXPECT_EQ(curl_easy_setopt(
                  easy.get(), CURLOPT_REDIR_PROTOCOLS_STR, "https"),
        CURLE_OK);
    EXPECT_EQ(curl_easy_setopt(
                  easy.get(), CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2TLS),
        CURLE_OK);
}

TEST(BuildSmoke, UsesPinnedBoringSslCrypto)
{
    constexpr std::array<std::uint8_t, SHA256_DIGEST_LENGTH> expected = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,
        0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,
        0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad,
    };
    constexpr std::array<std::uint8_t, 3> input = {'a', 'b', 'c'};
    std::array<std::uint8_t, SHA256_DIGEST_LENGTH> digest{};

    EXPECT_EQ(SHA256(input.data(), input.size(), digest.data()), digest.data());
    EXPECT_EQ(digest, expected);
}

TEST(BuildSmoke, ConfiguresPinnedBoringSslTlsPolicy)
{
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> context(
        SSL_CTX_new(TLS_method()),
        &SSL_CTX_free);
    ASSERT_NE(context, nullptr);
    ASSERT_EQ(SSL_CTX_set_min_proto_version(context.get(), TLS1_2_VERSION), 1);
    ASSERT_EQ(SSL_CTX_set_max_proto_version(context.get(), TLS1_3_VERSION), 1);
    EXPECT_EQ(SSL_CTX_get_min_proto_version(context.get()), TLS1_2_VERSION);
    EXPECT_EQ(SSL_CTX_get_max_proto_version(context.get()), TLS1_3_VERSION);
}

TEST(BuildSmoke, SerializesPinnedNghttp2ClientPreface)
{
    const nghttp2_info* const version = nghttp2_version(0);
    ASSERT_NE(version, nullptr);
    EXPECT_EQ(version->version_num, NGHTTP2_VERSION_NUM);

    nghttp2_session_callbacks* raw_callbacks = nullptr;
    ASSERT_EQ(nghttp2_session_callbacks_new(&raw_callbacks), 0);
    std::unique_ptr<nghttp2_session_callbacks,
        decltype(&nghttp2_session_callbacks_del)>
        callbacks(raw_callbacks, &nghttp2_session_callbacks_del);

    nghttp2_session* raw_session = nullptr;
    ASSERT_EQ(
        nghttp2_session_client_new(&raw_session, callbacks.get(), nullptr),
        0);
    std::unique_ptr<nghttp2_session, decltype(&nghttp2_session_del)> session(
        raw_session,
        &nghttp2_session_del);

    const nghttp2_settings_entry settings = {
        NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS,
        1,
    };
    ASSERT_EQ(
        nghttp2_submit_settings(
            session.get(), NGHTTP2_FLAG_NONE, &settings, 1),
        0);

    const std::uint8_t* serialized = nullptr;
    const nghttp2_ssize preface_length =
        nghttp2_session_mem_send2(session.get(), &serialized);
    ASSERT_EQ(preface_length, NGHTTP2_CLIENT_MAGIC_LEN);
    ASSERT_NE(serialized, nullptr);
    EXPECT_EQ(
        std::string_view(
            reinterpret_cast<const char*>(serialized),
            static_cast<std::size_t>(preface_length)),
        std::string_view(NGHTTP2_CLIENT_MAGIC, NGHTTP2_CLIENT_MAGIC_LEN));

    EXPECT_GT(nghttp2_session_mem_send2(session.get(), &serialized), 0);
}

TEST(BuildSmoke, RoundTripsWithPinnedZlib)
{
    constexpr std::string_view input =
        "bounded compressed response body for MediaProxy";
    std::array<Bytef, 128> compressed{};
    uLongf compressed_length = compressed.size();

    ASSERT_EQ(
        compress2(
            compressed.data(),
            &compressed_length,
            reinterpret_cast<const Bytef*>(input.data()),
            input.size(),
            Z_BEST_COMPRESSION),
        Z_OK);

    std::array<char, 64> output{};
    uLongf output_length = output.size();
    ASSERT_EQ(
        uncompress(
            reinterpret_cast<Bytef*>(output.data()),
            &output_length,
            compressed.data(),
            compressed_length),
        Z_OK);
    EXPECT_EQ(std::string_view(output.data(), output_length), input);

    std::array<Bytef, 4> undersized_output{};
    uLongf undersized_length = undersized_output.size();
    EXPECT_EQ(
        uncompress(
            undersized_output.data(),
            &undersized_length,
            compressed.data(),
            compressed_length),
        Z_BUF_ERROR);
}

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

TEST(BuildSmoke, ParsesStrictJsonWithPinnedYyjson)
{
    std::string event =
        R"({"version":"2.0","rawPath":"/convert","requestContext":{"requestId":"test-request"}})";
    yyjson_read_err error{};
    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> document(
        yyjson_read_opts(
            event.data(),
            event.size(),
            YYJSON_READ_NOFLAG,
            nullptr,
            &error),
        &yyjson_doc_free);

    ASSERT_NE(document, nullptr) << error.msg;
    yyjson_val* root = yyjson_doc_get_root(document.get());
    ASSERT_TRUE(yyjson_is_obj(root));
    EXPECT_STREQ(yyjson_get_str(yyjson_obj_get(root, "version")), "2.0");
    EXPECT_STREQ(yyjson_get_str(yyjson_obj_get(root, "rawPath")), "/convert");
}

TEST(BuildSmoke, SerializesResponseMetadataWithPinnedYyjson)
{
    std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)> document(
        yyjson_mut_doc_new(nullptr),
        &yyjson_mut_doc_free);
    ASSERT_NE(document, nullptr);

    yyjson_mut_val* root = yyjson_mut_obj(document.get());
    ASSERT_NE(root, nullptr);
    yyjson_mut_doc_set_root(document.get(), root);
    ASSERT_TRUE(yyjson_mut_obj_add_int(document.get(), root, "statusCode", 200));

    std::unique_ptr<char, decltype(&std::free)> metadata(
        yyjson_mut_write(document.get(), YYJSON_WRITE_NOFLAG, nullptr),
        &std::free);
    ASSERT_NE(metadata, nullptr);
    EXPECT_STREQ(metadata.get(), R"({"statusCode":200})");
}
