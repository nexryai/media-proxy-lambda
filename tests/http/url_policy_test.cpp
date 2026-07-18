#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>
#include <yyjson.h>

namespace {

using mediaproxy::http::AddressFamily;
using mediaproxy::http::UrlError;
using mediaproxy::http::validate_origin_url;

std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> LoadUrlVectors()
{
    const std::string path = std::string{MEDIAPROXY_SOURCE_DIR}
        + "/tests/vectors/url-policy.json";
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input.is_open()) << path;
    std::string json{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};

    yyjson_read_err error{};
    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> document(
        yyjson_read_opts(
            json.data(), json.size(), YYJSON_READ_NOFLAG, nullptr, &error),
        &yyjson_doc_free);
    EXPECT_NE(document, nullptr) << error.msg;
    return document;
}

} // namespace

TEST(UrlPolicy, MatchesCheckedInSyntaxAndAddressCorpus)
{
    const auto document = LoadUrlVectors();
    ASSERT_NE(document, nullptr);
    yyjson_val* const root = yyjson_doc_get_root(document.get());
    ASSERT_TRUE(yyjson_is_obj(root));
    yyjson_val* const cases = yyjson_obj_get(root, "cases");
    ASSERT_TRUE(yyjson_is_arr(cases));

    std::size_t index = 0;
    std::size_t maximum = 0;
    yyjson_val* vector = nullptr;
    yyjson_arr_foreach(cases, index, maximum, vector)
    {
        ASSERT_TRUE(yyjson_is_obj(vector));
        const char* const id = yyjson_get_str(yyjson_obj_get(vector, "id"));
        const char* const source =
            yyjson_get_str(yyjson_obj_get(vector, "url"));
        yyjson_val* const accepted_value =
            yyjson_obj_get(vector, "accepted");
        ASSERT_NE(id, nullptr);
        ASSERT_NE(source, nullptr);
        ASSERT_TRUE(yyjson_is_bool(accepted_value));
        SCOPED_TRACE(id);

        const auto result = validate_origin_url(source);
        const bool accepted = yyjson_get_bool(accepted_value);
        EXPECT_EQ(static_cast<bool>(result), accepted);
        EXPECT_EQ(result.error == UrlError::none, accepted);
        if (accepted) {
            ASSERT_TRUE(result.url.has_value());
            EXPECT_EQ(
                result.url->hostname,
                yyjson_get_str(yyjson_obj_get(vector, "hostname")));
            EXPECT_EQ(
                result.url->port,
                yyjson_get_uint(yyjson_obj_get(vector, "port")));
            EXPECT_EQ(
                result.url->request_target,
                yyjson_get_str(yyjson_obj_get(vector, "requestTarget")));
        }
    }
}

TEST(UrlPolicy, CanonicalizesIdnaBeforeDnsAndDropsFragments)
{
    const auto result =
        validate_origin_url("https://😀.example/a%2Fb?x=1#not-sent");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result.url.has_value());
    EXPECT_EQ(result.url->hostname, "xn--e28h.example");
    EXPECT_EQ(result.url->request_target, "/a%2Fb?x=1");
    EXPECT_EQ(
        result.url->canonical_url,
        "https://xn--e28h.example/a%2Fb?x=1");
    EXPECT_FALSE(result.url->literal_address.has_value());
}

TEST(UrlPolicy, RetainsValidatedLiteralAndExplicitPort)
{
    const auto result = validate_origin_url("https://1.1.1.1:80/image.png");
    ASSERT_TRUE(result);
    ASSERT_TRUE(result.url.has_value());
    ASSERT_TRUE(result.url->literal_address.has_value());
    EXPECT_EQ(
        result.url->literal_address->family,
        AddressFamily::ipv4);
    EXPECT_EQ(result.url->canonical_url, "https://1.1.1.1:80/image.png");
}

TEST(UrlPolicy, RejectsDelimiterAndTerminatorConfusion)
{
    EXPECT_FALSE(validate_origin_url("https://@origin.example/image.png"));
    EXPECT_FALSE(validate_origin_url("https://:secret@origin.example/image.png"));

    constexpr char embedded_source[] =
        "https://origin.example\0.attacker.example/image.png";
    const std::string embedded_nul{embedded_source, sizeof(embedded_source) - 1};
    const auto result = validate_origin_url(embedded_nul);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, UrlError::invalid_syntax);
}
