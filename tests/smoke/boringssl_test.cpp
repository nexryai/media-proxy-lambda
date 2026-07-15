#include <array>
#include <cstdint>
#include <memory>

#include <gtest/gtest.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>

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
