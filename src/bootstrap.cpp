#include <memory>
#include <string_view>

#include <openssl/ssl.h>

int main()
{
    constexpr std::string_view runtime_name = "mediaproxy-lambda";
    std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> tls_context(
        SSL_CTX_new(TLS_method()),
        &SSL_CTX_free);
    if (runtime_name.empty() || tls_context == nullptr) {
        return 1;
    }
    if (SSL_CTX_set_min_proto_version(tls_context.get(), TLS1_2_VERSION) != 1
            || SSL_CTX_set_max_proto_version(
                tls_context.get(), TLS1_3_VERSION) != 1) {
        return 1;
    }
    return 0;
}
