#include <cstdio>
#include <memory>
#include <string_view>

#include <curl/curl.h>
#include <jpeglib.h>
#include <lcms2.h>
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <png.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>
#include <webp/mux.h>
#include <webp/sharpyuv/sharpyuv.h>
#include <zlib.h>

extern "C" {
#include <nsgif.h>
}

namespace {

#define MEDIAPROXY_STRINGIFY_IMPL(value) #value
#define MEDIAPROXY_STRINGIFY(value) MEDIAPROXY_STRINGIFY_IMPL(value)

class CurlGlobal final {
public:
    CurlGlobal() noexcept
        : result_(curl_global_init(CURL_GLOBAL_DEFAULT))
    {
    }

    ~CurlGlobal()
    {
        if (ok()) {
            curl_global_cleanup();
        }
    }

    CurlGlobal(const CurlGlobal&) = delete;
    CurlGlobal& operator=(const CurlGlobal&) = delete;

    [[nodiscard]] bool ok() const noexcept
    {
        return result_ == CURLE_OK;
    }

private:
    CURLcode result_;
};

} // namespace

int main()
{
    constexpr std::string_view runtime_name = "mediaproxy-lambda";
    const CurlGlobal curl_global;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> tls_context(
        SSL_CTX_new(TLS_method()),
        &SSL_CTX_free);
    if (runtime_name.empty() || !curl_global.ok() || tls_context == nullptr) {
        return 1;
    }
    auto* volatile curl_version_function = &curl_version_info;
    const curl_version_info_data* const curl_version =
        curl_version_function(CURLVERSION_NOW);
    constexpr int required_curl_features =
        CURL_VERSION_SSL | CURL_VERSION_LIBZ | CURL_VERSION_HTTP2;
    if (curl_version == nullptr
            || curl_version->version_num != LIBCURL_VERSION_NUM
            || (curl_version->features & required_curl_features)
                != required_curl_features
            || curl_version->ssl_version == nullptr
            || !std::string_view{curl_version->ssl_version}.starts_with(
                "BoringSSL")) {
        return 1;
    }
    auto* volatile png_version_function = &png_access_version_number;
    if (png_version_function() != PNG_LIBPNG_VER) {
        return 1;
    }
    jpeg_error_mgr jpeg_errors{};
    auto* volatile jpeg_error_function = &jpeg_std_error;
    constexpr std::string_view required_jpeg_version =
        MEDIAPROXY_STRINGIFY(LIBJPEG_TURBO_VERSION);
    if (jpeg_error_function(&jpeg_errors) != &jpeg_errors
            || required_jpeg_version != "3.1.4.1") {
        return 1;
    }
    auto* volatile nsgif_error_function = &nsgif_strerror;
    if (std::string_view{nsgif_error_function(NSGIF_OK)} != "Success") {
        return 1;
    }
    auto* volatile lcms_version_function = &cmsGetEncodedCMMversion;
    if (lcms_version_function() != 2190) {
        return 1;
    }
    constexpr int required_webp_version = 0x010600;
    auto* volatile webp_decoder_version_function = &WebPGetDecoderVersion;
    auto* volatile webp_encoder_version_function = &WebPGetEncoderVersion;
    auto* volatile webp_demux_version_function = &WebPGetDemuxVersion;
    auto* volatile webp_mux_version_function = &WebPGetMuxVersion;
    auto* volatile sharpyuv_version_function = &SharpYuvGetVersion;
    if (webp_decoder_version_function() != required_webp_version
            || webp_encoder_version_function() != required_webp_version
            || webp_demux_version_function() != required_webp_version
            || webp_mux_version_function() != required_webp_version
            || sharpyuv_version_function() != SHARPYUV_VERSION) {
        return 1;
    }
    auto* volatile zlib_version_function = &zlibVersion;
    const char* const linked_zlib_version = zlib_version_function();
    if (linked_zlib_version == nullptr
            || std::string_view{linked_zlib_version} != ZLIB_VERSION) {
        return 1;
    }
    auto* volatile nghttp2_version_function = &nghttp2_version;
    const nghttp2_info* const http2_version_info =
        nghttp2_version_function(0);
    if (http2_version_info == nullptr
            || http2_version_info->version_num != NGHTTP2_VERSION_NUM) {
        return 1;
    }
    if (SSL_CTX_set_min_proto_version(tls_context.get(), TLS1_2_VERSION) != 1
            || SSL_CTX_set_max_proto_version(
                tls_context.get(), TLS1_3_VERSION) != 1) {
        return 1;
    }
    return 0;
}
