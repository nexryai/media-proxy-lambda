#include <cstdlib>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <yyjson.h>

TEST(BuildSmoke, ParsesStrictJsonWithPinnedYyjson)
{
    std::string event =
        R"({"version":"2.0","rawPath":"/convert","requestContext":{"requestId":"test-request"}})";
    yyjson_read_err error{};
    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> document(
        yyjson_read_opts(
            event.data(),
            event.size(),
            YYJSON_READ_NOFLAG,
            nullptr,
            &error),
        &yyjson_doc_free);

    ASSERT_NE(document, nullptr) << error.msg;
    yyjson_val* root = yyjson_doc_get_root(document.get());
    ASSERT_TRUE(yyjson_is_obj(root));
    EXPECT_STREQ(yyjson_get_str(yyjson_obj_get(root, "version")), "2.0");
    EXPECT_STREQ(yyjson_get_str(yyjson_obj_get(root, "rawPath")), "/convert");
}

TEST(BuildSmoke, SerializesResponseMetadataWithPinnedYyjson)
{
    std::unique_ptr<yyjson_mut_doc, decltype(&yyjson_mut_doc_free)> document(
        yyjson_mut_doc_new(nullptr),
        &yyjson_mut_doc_free);
    ASSERT_NE(document, nullptr);

    yyjson_mut_val* root = yyjson_mut_obj(document.get());
    ASSERT_NE(root, nullptr);
    yyjson_mut_doc_set_root(document.get(), root);
    ASSERT_TRUE(yyjson_mut_obj_add_int(document.get(), root, "statusCode", 200));

    std::unique_ptr<char, decltype(&std::free)> metadata(
        yyjson_mut_write(document.get(), YYJSON_WRITE_NOFLAG, nullptr),
        &std::free);
    ASSERT_NE(metadata, nullptr);
    EXPECT_STREQ(metadata.get(), R"({"statusCode":200})");
}
