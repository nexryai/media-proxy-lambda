#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <mediaproxy/http/query.hpp>
#include <yyjson.h>

namespace {

using mediaproxy::http::MediaSelector;
using mediaproxy::http::PreferredOutput;
using mediaproxy::http::parse_query;
using mediaproxy::http::select_media_options;

std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> LoadSelectorVectors()
{
    const std::string path =
        std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/vectors/selectors.json";
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

MediaSelector ParseSelector(std::string_view value)
{
    if (value == "avatar") {
        return MediaSelector::avatar;
    }
    if (value == "emoji") {
        return MediaSelector::emoji;
    }
    if (value == "preview") {
        return MediaSelector::preview;
    }
    if (value == "badge") {
        return MediaSelector::badge;
    }
    if (value == "thumbnail") {
        return MediaSelector::thumbnail;
    }
    if (value == "ticker") {
        return MediaSelector::ticker;
    }
    EXPECT_EQ(value, "default");
    return MediaSelector::default_media;
}

PreferredOutput ParseOutput(std::string_view value)
{
    if (value == "avif") {
        return PreferredOutput::avif;
    }
    EXPECT_EQ(value, "webp");
    return PreferredOutput::webp;
}

} // namespace

TEST(Selectors, MatchesCheckedInPrecedenceVectors)
{
    const auto document = LoadSelectorVectors();
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
        const char* const raw_query =
            yyjson_get_str(yyjson_obj_get(vector, "rawQuery"));
        const char* const selector =
            yyjson_get_str(yyjson_obj_get(vector, "selector"));
        const char* const output =
            yyjson_get_str(yyjson_obj_get(vector, "output"));
        ASSERT_NE(id, nullptr);
        ASSERT_NE(raw_query, nullptr);
        ASSERT_NE(selector, nullptr);
        ASSERT_NE(output, nullptr);
        SCOPED_TRACE(id);

        const auto options = select_media_options(parse_query(raw_query));
        EXPECT_EQ(options.selector, ParseSelector(selector));
        EXPECT_EQ(
            options.width_limit,
            yyjson_get_uint(yyjson_obj_get(vector, "width")));
        EXPECT_EQ(
            options.height_limit,
            yyjson_get_uint(yyjson_obj_get(vector, "height")));
        EXPECT_EQ(options.preferred_output, ParseOutput(output));
        EXPECT_EQ(
            options.force_static,
            yyjson_get_bool(yyjson_obj_get(vector, "static")));
    }
}
