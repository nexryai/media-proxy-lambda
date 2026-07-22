#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <arpa/inet.h>
#include <curl/curl.h>
#include <gtest/gtest.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/ssl.h>
#include <openssl/x509.h>
#include <sys/socket.h>
#include <unistd.h>
#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/origin_download.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/url_policy.hpp>
#include <zlib.h>

namespace {

using mediaproxy::http::AddressResolverApi;
using mediaproxy::http::OriginDownloadError;
using mediaproxy::http::OriginResponseAccumulator;
using mediaproxy::http::OriginTransportApi;
using mediaproxy::http::download_origin_once;
using mediaproxy::http::validate_origin_url;

template <typename Type, auto Destroy>
struct OwnedDeleter {
    void operator()(Type* value) const noexcept
    {
        if (value != nullptr) {
            static_cast<void>(Destroy(value));
        }
    }
};

template <typename Type, auto Destroy>
using Owned = std::unique_ptr<Type, OwnedDeleter<Type, Destroy>>;

class FileDescriptor final {
public:
    explicit FileDescriptor(int value = -1) noexcept
        : value_(value)
    {
    }

    ~FileDescriptor()
    {
        if (value_ >= 0) {
            ::close(value_);
        }
    }

    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    FileDescriptor(FileDescriptor&& other) noexcept
        : value_(std::exchange(other.value_, -1))
    {
    }
    FileDescriptor& operator=(FileDescriptor&& other) noexcept
    {
        if (this != &other) {
            if (value_ >= 0) {
                ::close(value_);
            }
            value_ = std::exchange(other.value_, -1);
        }
        return *this;
    }

    [[nodiscard]] int get() const noexcept { return value_; }

private:
    int value_;
};

struct TestIdentity {
    Owned<EVP_PKEY, &EVP_PKEY_free> key;
    Owned<X509, &X509_free> certificate;
    std::string certificate_pem;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return key != nullptr && certificate != nullptr
            && !certificate_pem.empty();
    }
};

[[nodiscard]] TestIdentity CreateIdentity(std::string_view hostname)
{
    TestIdentity identity;
    Owned<BIGNUM, &BN_free> exponent{BN_new()};
    Owned<RSA, &RSA_free> rsa{RSA_new()};
    identity.key.reset(EVP_PKEY_new());
    if (!exponent || !rsa || !identity.key
        || BN_set_word(exponent.get(), RSA_F4) != 1
        || RSA_generate_key_ex(rsa.get(), 2048, exponent.get(), nullptr) != 1
        || EVP_PKEY_assign_RSA(identity.key.get(), rsa.get()) != 1) {
        return {};
    }
    static_cast<void>(rsa.release());

    identity.certificate.reset(X509_new());
    if (!identity.certificate
        || X509_set_version(identity.certificate.get(), 2) != 1
        || ASN1_INTEGER_set(
               X509_get_serialNumber(identity.certificate.get()), 1)
            != 1
        || X509_gmtime_adj(
               X509_get_notBefore(identity.certificate.get()), -60)
            == nullptr
        || X509_gmtime_adj(
               X509_get_notAfter(identity.certificate.get()), 3600)
            == nullptr
        || X509_set_pubkey(
               identity.certificate.get(), identity.key.get())
            != 1) {
        return {};
    }
    X509_NAME* const subject =
        X509_get_subject_name(identity.certificate.get());
    const std::string terminated_hostname{hostname};
    if (subject == nullptr
        || X509_NAME_add_entry_by_txt(subject, "CN", MBSTRING_ASC,
               reinterpret_cast<const unsigned char*>(
                   terminated_hostname.c_str()),
               -1, -1, 0)
            != 1
        || X509_set_issuer_name(identity.certificate.get(), subject) != 1) {
        return {};
    }

    if (X509_sign(identity.certificate.get(), identity.key.get(),
               EVP_sha256())
            <= 0) {
        return {};
    }

    Owned<BIO, &BIO_free> pem{BIO_new(BIO_s_mem())};
    char* pem_bytes = nullptr;
    const long pem_size = pem
        ? PEM_write_bio_X509(pem.get(), identity.certificate.get())
                == 1
            ? BIO_get_mem_data(pem.get(), &pem_bytes)
            : 0
        : 0;
    if (pem_size <= 0 || pem_bytes == nullptr) {
        return {};
    }
    identity.certificate_pem.assign(
        pem_bytes, static_cast<std::size_t>(pem_size));
    return identity;
}

[[nodiscard]] FileDescriptor CreateListener(std::uint16_t& port)
{
    FileDescriptor listener{::socket(AF_INET, SOCK_STREAM, 0)};
    if (listener.get() < 0) {
        return listener;
    }
    const sockaddr_in address{
        .sin_family = AF_INET,
        .sin_port = 0,
        .sin_addr = {.s_addr = htonl(INADDR_LOOPBACK)},
        .sin_zero = {},
    };
    if (::bind(listener.get(), reinterpret_cast<const sockaddr*>(&address),
            sizeof(address)) != 0
        || ::listen(listener.get(), 1) != 0) {
        return FileDescriptor{};
    }
    sockaddr_in bound{};
    socklen_t bound_size = sizeof(bound);
    if (::getsockname(listener.get(), reinterpret_cast<sockaddr*>(&bound),
            &bound_size) != 0) {
        return FileDescriptor{};
    }
    port = ntohs(bound.sin_port);
    return listener;
}

[[nodiscard]] bool WriteAll(SSL* ssl, std::string_view bytes)
{
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const int request = static_cast<int>(std::min<std::size_t>(
            bytes.size() - offset, static_cast<std::size_t>(INT_MAX)));
        const int written = SSL_write(ssl, bytes.data() + offset, request);
        if (written <= 0) {
            return false;
        }
        offset += static_cast<std::size_t>(written);
    }
    return true;
}

class TlsOriginServer final {
public:
    TlsOriginServer(
        const TestIdentity& identity,
        std::span<const std::byte> body,
        std::string extra_headers = {})
        : body_(body.begin(), body.end())
        , extra_headers_(std::move(extra_headers))
        , context_(SSL_CTX_new(TLS_server_method()))
    {
        if (!context_
            || SSL_CTX_use_certificate(
                   context_.get(), identity.certificate.get())
                != 1
            || SSL_CTX_use_PrivateKey(context_.get(), identity.key.get()) != 1
            || SSL_CTX_check_private_key(context_.get()) != 1) {
            return;
        }
        listener_ = CreateListener(port_);
        if (listener_.get() < 0) {
            return;
        }
        thread_ = std::thread{[this] { Serve(); }};
    }

    ~TlsOriginServer()
    {
        Finish();
    }

    TlsOriginServer(const TlsOriginServer&) = delete;
    TlsOriginServer& operator=(const TlsOriginServer&) = delete;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return context_ != nullptr && listener_.get() >= 0 && port_ != 0;
    }

    [[nodiscard]] std::uint16_t port() const noexcept { return port_; }
    [[nodiscard]] bool served() const noexcept { return served_.load(); }

    void Finish() noexcept
    {
        if (thread_.joinable()) {
            ::shutdown(listener_.get(), SHUT_RDWR);
            thread_.join();
        }
    }

private:
    void Serve() noexcept
    {
        FileDescriptor client{::accept(listener_.get(), nullptr, nullptr)};
        if (client.get() < 0) {
            return;
        }
        Owned<SSL, &SSL_free> ssl{SSL_new(context_.get())};
        if (!ssl || SSL_set_fd(ssl.get(), client.get()) != 1
            || SSL_accept(ssl.get()) != 1) {
            return;
        }
        std::string request;
        std::array<char, 1024> buffer{};
        while (request.size() <= 64U * 1024U
            && request.find("\r\n\r\n") == std::string::npos) {
            const int received =
                SSL_read(ssl.get(), buffer.data(), buffer.size());
            if (received <= 0) {
                return;
            }
            request.append(buffer.data(), static_cast<std::size_t>(received));
        }
        if (!request.starts_with("GET /image.png HTTP/1.1\r\n")) {
            return;
        }
        std::string response = "HTTP/1.1 200 OK\r\nContent-Length: ";
        response += std::to_string(body_.size());
        response += "\r\nContent-Type: image/png\r\n";
        response += extra_headers_;
        response += "Connection: close\r\n\r\n";
        response.append(reinterpret_cast<const char*>(body_.data()),
            body_.size());
        served_.store(WriteAll(ssl.get(), response));
        static_cast<void>(SSL_shutdown(ssl.get()));
    }

    std::vector<std::byte> body_;
    std::string extra_headers_;
    Owned<SSL_CTX, &SSL_CTX_free> context_;
    FileDescriptor listener_;
    std::uint16_t port_ = 0;
    std::atomic<bool> served_ = false;
    std::thread thread_;
};

sockaddr_in public_address{};
addrinfo public_answer{};

int PublicLookup(
    const char*,
    const char*,
    const addrinfo*,
    addrinfo** result)
{
    if (result == nullptr) {
        return EAI_FAIL;
    }
    public_address = {
        .sin_family = AF_INET,
        .sin_port = htons(443),
        .sin_addr = {},
        .sin_zero = {},
    };
    if (inet_pton(AF_INET, "1.1.1.1", &public_address.sin_addr) != 1) {
        return EAI_FAIL;
    }
    public_answer = {
        .ai_flags = 0,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_addrlen = sizeof(public_address),
        .ai_addr = reinterpret_cast<sockaddr*>(&public_address),
        .ai_canonname = nullptr,
        .ai_next = nullptr,
    };
    *result = &public_answer;
    return 0;
}

void PublicRelease(addrinfo*)
{
}

struct LocalTlsTransport {
    std::string hostname;
    std::uint16_t port = 0;
    std::string ca_pem;
};

CURL* CreateEasy(void*)
{
    return curl_easy_init();
}

void DestroyEasy(CURL* easy, void*)
{
    curl_easy_cleanup(easy);
}

CURLcode PerformLocalTls(
    CURL* easy,
    OriginResponseAccumulator&,
    void* context)
{
    auto& transport = *static_cast<LocalTlsTransport*>(context);
    const std::string mapping = transport.hostname + ":443:127.0.0.1:"
        + std::to_string(transport.port);
    curl_slist* raw_mapping = curl_slist_append(nullptr, mapping.c_str());
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> connect_to(
        raw_mapping, &curl_slist_free_all);
    if (!connect_to) {
        return CURLE_OUT_OF_MEMORY;
    }
    curl_blob ca_blob{
        .data = transport.ca_pem.data(),
        .len = transport.ca_pem.size(),
        .flags = CURL_BLOB_COPY,
    };
    if (curl_easy_setopt(easy, CURLOPT_CONNECT_TO, connect_to.get()) != CURLE_OK
        || curl_easy_setopt(easy, CURLOPT_CAINFO_BLOB, &ca_blob) != CURLE_OK) {
        return CURLE_FAILED_INIT;
    }
    return curl_easy_perform(easy);
}

CURLcode ResponseCode(CURL* easy, long* status, void*)
{
    return curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, status);
}

OriginTransportApi Transport(LocalTlsTransport& transport)
{
    return {
        .context = &transport,
        .create = &CreateEasy,
        .destroy = &DestroyEasy,
        .perform = &PerformLocalTls,
        .response_code = &ResponseCode,
    };
}

std::vector<std::byte> ReadFile(const std::string& path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        return {};
    }
    const std::string bytes{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
    const auto body = std::as_bytes(std::span{bytes});
    return {body.begin(), body.end()};
}

std::vector<std::byte> Gzip(std::span<const std::byte> input)
{
    if (input.size() > std::numeric_limits<uInt>::max()) {
        return {};
    }
    z_stream stream{};
    if (deflateInit2(&stream, Z_BEST_SPEED, Z_DEFLATED, 15 + 16, 8,
            Z_DEFAULT_STRATEGY)
        != Z_OK) {
        return {};
    }
    struct DeflateCleanup {
        z_stream* stream;
        ~DeflateCleanup()
        {
            static_cast<void>(deflateEnd(stream));
        }
    } cleanup{.stream = &stream};
    std::vector<std::byte> output(compressBound(input.size()) + 32U);
    if (output.size() > std::numeric_limits<uInt>::max()) {
        return {};
    }
    stream.next_in = reinterpret_cast<Bytef*>(
        const_cast<std::byte*>(input.data()));
    stream.avail_in = static_cast<uInt>(input.size());
    stream.next_out = reinterpret_cast<Bytef*>(output.data());
    stream.avail_out = static_cast<uInt>(output.size());
    const int result = deflate(&stream, Z_FINISH);
    if (result != Z_STREAM_END) {
        return {};
    }
    output.resize(stream.total_out);
    return output;
}

class OriginTlsTest : public testing::Test {
protected:
    static void SetUpTestSuite()
    {
        ASSERT_EQ(curl_global_init(CURL_GLOBAL_DEFAULT), CURLE_OK);
    }

    static void TearDownTestSuite()
    {
        curl_global_cleanup();
    }
};

TEST_F(OriginTlsTest, DownloadsFromPinnedHostWithPeerVerification)
{
    const TestIdentity identity = CreateIdentity("origin.example");
    ASSERT_TRUE(identity);
    const std::vector<std::byte> body = ReadFile(
        std::string{MEDIAPROXY_SOURCE_DIR}
        + "/tests/fixtures/media/apng/palette-static.png");
    ASSERT_FALSE(body.empty());
    TlsOriginServer server{identity, body};
    ASSERT_TRUE(server);

    const auto origin =
        validate_origin_url("https://origin.example/image.png");
    ASSERT_TRUE(origin);
    LocalTlsTransport transport{
        .hostname = "origin.example",
        .port = server.port(),
        .ca_pem = identity.certificate_pem,
    };
    const auto downloaded = download_origin_once(*origin.url, 5'000,
        AddressResolverApi{.lookup = &PublicLookup, .release = &PublicRelease},
        Transport(transport));
    server.Finish();

    ASSERT_TRUE(downloaded) << static_cast<int>(downloaded.error) << ": "
                            << curl_easy_strerror(downloaded.curl_error);
    EXPECT_EQ(downloaded.status, 200);
    EXPECT_EQ(downloaded.response.body(), body);
    EXPECT_TRUE(server.served());
}

TEST_F(OriginTlsTest, RejectsCertificateForDifferentPinnedHostname)
{
    const TestIdentity identity = CreateIdentity("origin.example");
    ASSERT_TRUE(identity);
    const std::array<std::byte, 1> body{std::byte{0}};
    TlsOriginServer server{identity, body};
    ASSERT_TRUE(server);

    const auto origin =
        validate_origin_url("https://different.example/image.png");
    ASSERT_TRUE(origin);
    LocalTlsTransport transport{
        .hostname = "different.example",
        .port = server.port(),
        .ca_pem = identity.certificate_pem,
    };
    const auto downloaded = download_origin_once(*origin.url, 5'000,
        AddressResolverApi{.lookup = &PublicLookup, .release = &PublicRelease},
        Transport(transport));
    server.Finish();

    EXPECT_FALSE(downloaded);
    EXPECT_EQ(downloaded.error, OriginDownloadError::transfer);
    EXPECT_EQ(downloaded.curl_error, CURLE_PEER_FAILED_VERIFICATION);
}

TEST_F(OriginTlsTest, RejectsUntrustedCertificateChain)
{
    const TestIdentity identity = CreateIdentity("origin.example");
    const TestIdentity untrusted_identity = CreateIdentity("origin.example");
    ASSERT_TRUE(identity);
    ASSERT_TRUE(untrusted_identity);
    const std::array<std::byte, 1> body{std::byte{0}};
    TlsOriginServer server{identity, body};
    ASSERT_TRUE(server);

    const auto origin =
        validate_origin_url("https://origin.example/image.png");
    ASSERT_TRUE(origin);
    LocalTlsTransport transport{
        .hostname = "origin.example",
        .port = server.port(),
        .ca_pem = untrusted_identity.certificate_pem,
    };
    const auto downloaded = download_origin_once(*origin.url, 5'000,
        AddressResolverApi{.lookup = &PublicLookup, .release = &PublicRelease},
        Transport(transport));
    server.Finish();

    EXPECT_FALSE(downloaded);
    EXPECT_EQ(downloaded.error, OriginDownloadError::transfer);
    EXPECT_EQ(downloaded.curl_error, CURLE_PEER_FAILED_VERIFICATION);
}

TEST_F(OriginTlsTest, TransparentlyDecodesGzipBeforeReturningBody)
{
    const TestIdentity identity = CreateIdentity("origin.example");
    ASSERT_TRUE(identity);
    const std::vector<std::byte> body = ReadFile(
        std::string{MEDIAPROXY_SOURCE_DIR}
        + "/tests/fixtures/media/apng/palette-static.png");
    const std::vector<std::byte> compressed = Gzip(body);
    ASSERT_FALSE(compressed.empty());
    TlsOriginServer server{
        identity, compressed, "Content-Encoding: gzip\r\n"};
    ASSERT_TRUE(server);

    const auto origin =
        validate_origin_url("https://origin.example/image.png");
    ASSERT_TRUE(origin);
    LocalTlsTransport transport{
        .hostname = "origin.example",
        .port = server.port(),
        .ca_pem = identity.certificate_pem,
    };
    const auto downloaded = download_origin_once(*origin.url, 5'000,
        AddressResolverApi{.lookup = &PublicLookup, .release = &PublicRelease},
        Transport(transport));
    server.Finish();

    ASSERT_TRUE(downloaded) << curl_easy_strerror(downloaded.curl_error);
    EXPECT_EQ(downloaded.response.body(), body);
    EXPECT_TRUE(server.served());
}

} // namespace
