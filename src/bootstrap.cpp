#include <memory>
#include <string_view>

#include <nghttp2/nghttp2.h>
#include <openssl/ssl.h>
#include <zlib.h>

int main()
{
    constexpr std::string_view runtime_name = "mediaproxy-lambda";
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> tls_context(
        SSL_CTX_new(TLS_method()),
        &SSL_CTX_free);
    if (runtime_name.empty() || tls_context == nullptr) {
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
