#include <array>
#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <mediaproxy/http/query.hpp>
#include <yyjson.h>

namespace {

using mediaproxy::http::parse_query;
using mediaproxy::http::select_media_options;

auto load_query_vectors() -> std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> {
    const std::string path = std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/vectors/query.json";
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input.is_open()) << path;
    std::string json{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};

    yyjson_read_err error{};
    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> document(yyjson_read_opts(json.data(), json.size(), YYJSON_READ_NOFLAG, nullptr, &error), &yyjson_doc_free);
    EXPECT_NE(document, nullptr) << error.msg;

    return document;
}

auto to_hex(const std::string &value) -> std::string {
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const unsigned char byte : value) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }

    return output.str();
}

} // namespace

TEST(Query, MatchesCheckedInParsingLookupAndBooleanVectors) {
    const auto document = load_query_vectors();
    ASSERT_NE(document, nullptr);
    yyjson_val *const root = yyjson_doc_get_root(document.get());
    ASSERT_TRUE(yyjson_is_obj(root));
    yyjson_val *const cases = yyjson_obj_get(root, "cases");
    ASSERT_TRUE(yyjson_is_arr(cases));

    std::size_t case_index = 0;
    std::size_t case_maximum = 0;
    yyjson_val *vector = nullptr;
    yyjson_arr_foreach(cases, case_index, case_maximum, vector) {
        ASSERT_TRUE(yyjson_is_obj(vector));
        const char *const id = yyjson_get_str(yyjson_obj_get(vector, "id"));
        const char *const raw = yyjson_get_str(yyjson_obj_get(vector, "raw"));
        ASSERT_NE(id, nullptr);
        ASSERT_NE(raw, nullptr);
        SCOPED_TRACE(id);

        const auto parameters = parse_query(raw);
        if (yyjson_val *const expected = yyjson_obj_get(vector, "expected")) {
            ASSERT_TRUE(yyjson_is_arr(expected));
            ASSERT_EQ(parameters.entries().size(), yyjson_arr_size(expected));

            std::size_t entry_index = 0;
            std::size_t entry_maximum = 0;
            yyjson_val *entry = nullptr;
            yyjson_arr_foreach(expected, entry_index, entry_maximum, entry) {
                ASSERT_TRUE(yyjson_is_obj(entry));
                const char *const key_hex = yyjson_get_str(yyjson_obj_get(entry, "keyHex"));
                const char *const value_hex = yyjson_get_str(yyjson_obj_get(entry, "valueHex"));
                ASSERT_NE(key_hex, nullptr);
                ASSERT_NE(value_hex, nullptr);
                EXPECT_EQ(to_hex(parameters.entries()[entry_index].key), key_hex);
                EXPECT_EQ(to_hex(parameters.entries()[entry_index].value), value_hex);
            }
        }

        if (yyjson_val *const lookups = yyjson_obj_get(vector, "lookups")) {
            ASSERT_TRUE(yyjson_is_obj(lookups));
            std::size_t lookup_index = 0;
            std::size_t lookup_maximum = 0;
            yyjson_val *key = nullptr;
            yyjson_val *value = nullptr;
            yyjson_obj_foreach(lookups, lookup_index, lookup_maximum, key, value) {
                ASSERT_TRUE(yyjson_is_str(key));
                ASSERT_TRUE(yyjson_is_str(value));
                EXPECT_EQ(parameters.first(yyjson_get_str(key)), yyjson_get_str(value));
            }
        }

        if (yyjson_val *const booleans = yyjson_obj_get(vector, "expectedBooleans")) {
            ASSERT_TRUE(yyjson_is_obj(booleans));
            std::size_t boolean_index = 0;
            std::size_t boolean_maximum = 0;
            yyjson_val *key = nullptr;
            yyjson_val *value = nullptr;
            yyjson_obj_foreach(booleans, boolean_index, boolean_maximum, key, value) {
                ASSERT_TRUE(yyjson_is_str(key));
                ASSERT_TRUE(yyjson_is_bool(value));
                EXPECT_EQ(parameters.boolean(yyjson_get_str(key)), yyjson_get_bool(value));
            }
        }
    }
}

TEST(Query, UrlOnlyQualityUsesAcceptedEntries) {
    struct Case {
        std::string_view query;
        bool url_only;
    };
    constexpr std::array cases{
        Case{.query = "url=https%3A%2F%2Fexample.com%2Fx", .url_only = true},
        Case{.query = "url=x&&", .url_only = true},
        Case{.query = "url=x&broken=%GG", .url_only = true},
        Case{.query = "url=x&url=y", .url_only = false},
        Case{.query = "url=x&unused=1", .url_only = false},
        Case{.query = "url=x&static=0", .url_only = false},
        Case{.query = "url=x&avatar=1", .url_only = false},
        Case{.query = "avatar=1", .url_only = false},
    };
    for (const auto &test_case : cases) {
        SCOPED_TRACE(test_case.query);
        EXPECT_EQ(select_media_options(parse_query(test_case.query)).url_only, test_case.url_only);
    }
}
