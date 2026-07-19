#include <cstdio>
#include <memory>
#include <string_view>

#include <aom/aom_codec.h>
#include <aom/aom_encoder.h>
#include <aom/aomcx.h>
#include <aom/aomdx.h>
#include <curl/curl.h>
#include <expat.h>
#include <ffi.h>
#include <gio/gio.h>
#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <jpeglib.h>
#include <lcms2.h>
#include <libexif/exif-tag.h>
#include <libheif/heif.h>
#include <mediaproxy/http/ca_bundle.hpp>
#include <mediaproxy/http/idna.hpp>
#include <mediaproxy/media/vips_runtime.hpp>
#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <pcre2.h>
#include <png.h>
#include <webp/decode.h>
#include <webp/demux.h>
#include <webp/encode.h>
#include <webp/mux.h>
#include <webp/sharpyuv/sharpyuv.h>
#include <vips/vips.h>
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
    const auto idna_hostname =
        mediaproxy::http::normalize_hostname("bücher.example");
    const auto ca_bundle = mediaproxy::http::embedded_ca_bundle();
    const CurlGlobal curl_global;
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> tls_context(
        SSL_CTX_new(TLS_method()),
        &SSL_CTX_free);
    if (runtime_name.empty()
            || !idna_hostname
            || idna_hostname.ascii != "xn--bcher-kva.example"
            || ca_bundle.empty()
            || !curl_global.ok()
            || tls_context == nullptr) {
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
    auto* volatile exif_tag_name_function = &exif_tag_get_name;
    const char* const orientation_tag_name =
        exif_tag_name_function(EXIF_TAG_ORIENTATION);
    if (orientation_tag_name == nullptr
        || std::string_view{orientation_tag_name} != "Orientation") {
        return 1;
    }
    auto* volatile expat_version_function = &XML_ExpatVersion;
    const XML_LChar* const expat_version = expat_version_function();
    if (expat_version == nullptr
        || std::string_view{expat_version} != "expat_2.8.2") {
        return 1;
    }
    ffi_cif ffi_call_interface{};
    ffi_type* ffi_argument_types[] = {
        &ffi_type_sint32,
        &ffi_type_sint32,
    };
    auto* volatile ffi_prepare_function = &ffi_prep_cif;
    if (std::string_view{FFI_VERSION_STRING} != "3.7.1"
            || ffi_prepare_function(&ffi_call_interface, FFI_DEFAULT_ABI,
                2, &ffi_type_sint32, ffi_argument_types)
                != FFI_OK) {
        return 1;
    }
    if (glib_major_version != GLIB_MAJOR_VERSION
            || glib_minor_version != GLIB_MINOR_VERSION
            || glib_micro_version != GLIB_MICRO_VERSION
            || g_module_supported()) {
        return 1;
    }
    constexpr std::string_view glib_probe = "gio";
    std::unique_ptr<GInputStream, decltype(&g_object_unref)> glib_stream(
        g_memory_input_stream_new_from_data(
            glib_probe.data(), glib_probe.size(), nullptr),
        &g_object_unref);
    char glib_buffer[3]{};
    if (glib_stream == nullptr
            || g_input_stream_read(glib_stream.get(), glib_buffer,
                   sizeof(glib_buffer), nullptr, nullptr)
                != static_cast<gssize>(sizeof(glib_buffer))
            || std::string_view{glib_buffer, sizeof(glib_buffer)}
                != glib_probe) {
        return 1;
    }
    auto* volatile aom_version_function = &aom_codec_version;
    auto* volatile aom_encoder_interface_function = &aom_codec_av1_cx;
    auto* volatile aom_decoder_interface_function = &aom_codec_av1_dx;
    auto* volatile aom_encoder_config_function = &aom_codec_enc_config_default;
    aom_codec_iface_t* const aom_encoder_interface =
        aom_encoder_interface_function();
    aom_codec_enc_cfg_t aom_encoder_config{};
    if (aom_version_function() != ((3 << 16) | (14 << 8) | 1)
            || aom_encoder_interface == nullptr
            || aom_decoder_interface_function() == nullptr
            || aom_encoder_config_function(aom_encoder_interface,
                   &aom_encoder_config, AOM_USAGE_ALL_INTRA)
                != AOM_CODEC_OK) {
        return 1;
    }
    auto* volatile heif_version_function = &heif_get_version_number;
    auto* volatile heif_init_function = &heif_init;
    auto* volatile heif_deinit_function = &heif_deinit;
    auto* volatile heif_decoder_function = &heif_have_decoder_for_format;
    auto* volatile heif_encoder_function = &heif_have_encoder_for_format;
    if (heif_version_function() != LIBHEIF_NUMERIC_VERSION
            || heif_init_function(nullptr).code != heif_error_Ok
            || heif_decoder_function(heif_compression_AV1) == 0
            || heif_encoder_function(heif_compression_AV1) == 0
            || heif_decoder_function(heif_compression_HEVC) != 0
            || heif_encoder_function(heif_compression_HEVC) != 0) {
        return 1;
    }
    heif_deinit_function();
    if (!mediaproxy::media::initialize_vips()) {
        return 1;
    }
    if (vips_type_find("VipsOperation", "heifload") == 0
            || vips_type_find("VipsOperation", "heifsave") == 0
            || vips_type_find("VipsOperation", "tiffload") != 0
            || vips_type_find("VipsOperation", "magickload") != 0) {
        return 1;
    }
    auto* volatile pcre2_config_function = &pcre2_config;
    PCRE2_UCHAR pcre2_version[32] = {};
    if (pcre2_config_function(PCRE2_CONFIG_VERSION, pcre2_version) < 0
        || !std::string_view{reinterpret_cast<const char*>(pcre2_version)}
                .starts_with("10.47 ")) {
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
