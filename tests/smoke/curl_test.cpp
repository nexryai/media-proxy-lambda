#include <array>
#include <memory>
#include <string_view>

#include <curl/curl.h>
#include <gtest/gtest.h>

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

} // namespace

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
