#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <mediaproxy/http/idna.hpp>
#include <yyjson.h>

namespace {

using mediaproxy::http::HostnameError;
using mediaproxy::http::normalize_hostname;

std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> LoadIdnaVectors()
{
    const std::string path =
        std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/vectors/idna.json";
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

TEST(Idna, MatchesCheckedInUts46Corpus)
{
    const auto document = LoadIdnaVectors();
    ASSERT_NE(document, nullptr);
    yyjson_val* const root = yyjson_doc_get_root(document.get());
    ASSERT_TRUE(yyjson_is_obj(root));
    yyjson_val* const cases = yyjson_obj_get(root, "cases");
    ASSERT_TRUE(yyjson_is_arr(cases));

    std::size_t index = 0;
    std::size_t maximum = 0;
    yyjson_val* value = nullptr;
    yyjson_arr_foreach(cases, index, maximum, value)
    {
        ASSERT_TRUE(yyjson_is_obj(value));
        const char* const id = yyjson_get_str(yyjson_obj_get(value, "id"));
        const char* const input =
            yyjson_get_str(yyjson_obj_get(value, "input"));
        yyjson_val* const accepted_value =
            yyjson_obj_get(value, "accepted");
        ASSERT_NE(id, nullptr);
        ASSERT_NE(input, nullptr);
        ASSERT_TRUE(yyjson_is_bool(accepted_value));
        SCOPED_TRACE(id);

        const auto result = normalize_hostname(input);
        const bool accepted = yyjson_get_bool(accepted_value);
        EXPECT_EQ(static_cast<bool>(result), accepted);
        if (accepted) {
            const char* const expected =
                yyjson_get_str(yyjson_obj_get(value, "ascii"));
            ASSERT_NE(expected, nullptr);
            EXPECT_EQ(result.ascii, expected);
            EXPECT_EQ(result.error, HostnameError::none);
        } else {
            EXPECT_TRUE(result.ascii.empty());
            EXPECT_NE(result.error, HostnameError::none);
        }
    }
}

TEST(Idna, RejectsInvalidUtf8BeforeDnsUse)
{
    const std::string invalid_utf8{"origin\xc0\xaf.example", 16};
    const auto result = normalize_hostname(invalid_utf8);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, HostnameError::idna_conversion);
}

TEST(Idna, EnforcesBoundedInputAndDnsWireLengths)
{
    const auto oversized_input = normalize_hostname(std::string(4097, 'a'));
    EXPECT_FALSE(oversized_input);
    EXPECT_EQ(oversized_input.error, HostnameError::input_too_long);

    const auto oversized_label =
        normalize_hostname(std::string(64, 'a') + ".example");
    EXPECT_FALSE(oversized_label);
    EXPECT_EQ(oversized_label.error, HostnameError::label_too_long);

    const std::string maximum_hostname =
        std::string(63, 'a') + "."
        + std::string(63, 'b') + "."
        + std::string(63, 'c') + "."
        + std::string(61, 'd');
    ASSERT_EQ(maximum_hostname.size(), 253U);
    EXPECT_TRUE(normalize_hostname(maximum_hostname));
    EXPECT_TRUE(normalize_hostname(maximum_hostname + "."));

    const std::string oversized_hostname =
        std::string(63, 'a') + "."
        + std::string(63, 'b') + "."
        + std::string(63, 'c') + "."
        + std::string(62, 'd');
    ASSERT_EQ(oversized_hostname.size(), 254U);
    const auto oversized_result = normalize_hostname(oversized_hostname);
    EXPECT_FALSE(oversized_result);
    EXPECT_EQ(oversized_result.error, HostnameError::hostname_too_long);
}
