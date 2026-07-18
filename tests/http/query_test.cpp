#include <cstddef>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>
#include <mediaproxy/http/query.hpp>
#include <yyjson.h>

namespace {

using mediaproxy::http::parse_query;

std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> LoadQueryVectors()
{
    const std::string path =
        std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/vectors/query.json";
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

std::string ToHex(const std::string& value)
{
    std::ostringstream output;
    output << std::hex << std::setfill('0');
    for (const unsigned char byte : value) {
        output << std::setw(2) << static_cast<unsigned int>(byte);
    }
    return output.str();
}

} // namespace

TEST(Query, MatchesCheckedInParsingLookupAndBooleanVectors)
{
    const auto document = LoadQueryVectors();
    ASSERT_NE(document, nullptr);
    yyjson_val* const root = yyjson_doc_get_root(document.get());
    ASSERT_TRUE(yyjson_is_obj(root));
    yyjson_val* const cases = yyjson_obj_get(root, "cases");
    ASSERT_TRUE(yyjson_is_arr(cases));

    std::size_t case_index = 0;
    std::size_t case_maximum = 0;
    yyjson_val* vector = nullptr;
    yyjson_arr_foreach(cases, case_index, case_maximum, vector)
    {
        ASSERT_TRUE(yyjson_is_obj(vector));
        const char* const id = yyjson_get_str(yyjson_obj_get(vector, "id"));
        const char* const raw = yyjson_get_str(yyjson_obj_get(vector, "raw"));
        ASSERT_NE(id, nullptr);
        ASSERT_NE(raw, nullptr);
        SCOPED_TRACE(id);

        const auto parameters = parse_query(raw);
        if (yyjson_val* const expected = yyjson_obj_get(vector, "expected")) {
            ASSERT_TRUE(yyjson_is_arr(expected));
            ASSERT_EQ(parameters.entries().size(), yyjson_arr_size(expected));

            std::size_t entry_index = 0;
            std::size_t entry_maximum = 0;
            yyjson_val* entry = nullptr;
            yyjson_arr_foreach(
                expected, entry_index, entry_maximum, entry)
            {
                ASSERT_TRUE(yyjson_is_obj(entry));
                const char* const key_hex =
                    yyjson_get_str(yyjson_obj_get(entry, "keyHex"));
                const char* const value_hex =
                    yyjson_get_str(yyjson_obj_get(entry, "valueHex"));
                ASSERT_NE(key_hex, nullptr);
                ASSERT_NE(value_hex, nullptr);
                EXPECT_EQ(ToHex(parameters.entries()[entry_index].key), key_hex);
                EXPECT_EQ(
                    ToHex(parameters.entries()[entry_index].value), value_hex);
            }
        }

        if (yyjson_val* const lookups = yyjson_obj_get(vector, "lookups")) {
            ASSERT_TRUE(yyjson_is_obj(lookups));
            std::size_t lookup_index = 0;
            std::size_t lookup_maximum = 0;
            yyjson_val* key = nullptr;
            yyjson_val* value = nullptr;
            yyjson_obj_foreach(
                lookups, lookup_index, lookup_maximum, key, value)
            {
                ASSERT_TRUE(yyjson_is_str(key));
                ASSERT_TRUE(yyjson_is_str(value));
                EXPECT_EQ(
                    parameters.first(yyjson_get_str(key)),
                    yyjson_get_str(value));
            }
        }

        if (yyjson_val* const booleans =
                yyjson_obj_get(vector, "expectedBooleans")) {
            ASSERT_TRUE(yyjson_is_obj(booleans));
            std::size_t boolean_index = 0;
            std::size_t boolean_maximum = 0;
            yyjson_val* key = nullptr;
            yyjson_val* value = nullptr;
            yyjson_obj_foreach(
                booleans, boolean_index, boolean_maximum, key, value)
            {
                ASSERT_TRUE(yyjson_is_str(key));
                ASSERT_TRUE(yyjson_is_bool(value));
                EXPECT_EQ(
                    parameters.boolean(yyjson_get_str(key)),
                    yyjson_get_bool(value));
            }
        }
    }
}
