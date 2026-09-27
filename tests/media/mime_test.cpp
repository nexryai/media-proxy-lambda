#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <mediaproxy/media/mime.hpp>
#include <yyjson.h>

namespace {

using mediaproxy::media::mime_type_name;
using mediaproxy::media::sniff_mime;

auto load_mime_vectors() -> std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> {
    const std::string path = std::string{MEDIAPROXY_SOURCE_DIR} + "/tests/vectors/mime.json";
    std::ifstream input(path, std::ios::binary);
    EXPECT_TRUE(input.is_open()) << path;
    std::string json{std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};

    yyjson_read_err error{};
    std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> document(yyjson_read_opts(json.data(), json.size(), YYJSON_READ_NOFLAG, nullptr, &error), &yyjson_doc_free);
    EXPECT_NE(document, nullptr) << error.msg;

    return document;
}

auto hex_nibble(char value) -> unsigned char {
    if (value >= '0' && value <= '9') {
        return static_cast<unsigned char>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<unsigned char>(value - 'a' + 10);
    }
    EXPECT_GE(value, 'A');
    EXPECT_LE(value, 'F');

    return static_cast<unsigned char>(value - 'A' + 10);
}

auto decode_hex(std::string_view hex) -> std::vector<std::byte> {
    EXPECT_EQ(hex.size() % 2, 0U);
    std::vector<std::byte> decoded;
    decoded.reserve(hex.size() / 2);
    for (std::size_t offset = 0; offset + 1 < hex.size(); offset += 2) {
        const auto value = static_cast<unsigned char>((hex_nibble(hex[offset]) << 4U) | hex_nibble(hex[offset + 1]));
        decoded.push_back(static_cast<std::byte>(value));
    }

    return decoded;
}

auto utf8_sample(std::string_view text) -> std::vector<std::byte> {
    const auto *bytes = reinterpret_cast<const std::byte *>(text.data());

    return {bytes, bytes + text.size()};
}

void expect_mime(std::span<const std::byte> sample, std::string_view expected) {
    EXPECT_EQ(mime_type_name(sniff_mime(sample)), expected);
}

void check_string_variants(yyjson_val *variants, std::string_view expected, bool hexadecimal) {
    ASSERT_TRUE(yyjson_is_arr(variants));
    std::size_t index = 0;
    std::size_t maximum = 0;
    yyjson_val *value = nullptr;
    yyjson_arr_foreach(variants, index, maximum, value) {
        const char *text = yyjson_get_str(value);
        ASSERT_NE(text, nullptr);
        const std::vector<std::byte> sample = hexadecimal ? decode_hex(text) : utf8_sample(text);
        expect_mime(sample, expected);
    }
}

void check_generated_sample(yyjson_val *generated, std::string_view expected) {
    ASSERT_TRUE(yyjson_is_obj(generated));
    const char *prefix_hex = yyjson_get_str(yyjson_obj_get(generated, "prefixByteHex"));
    const char *suffix_hex = yyjson_get_str(yyjson_obj_get(generated, "suffixHex"));
    yyjson_val *const count_value = yyjson_obj_get(generated, "prefixCount");
    ASSERT_NE(prefix_hex, nullptr);
    ASSERT_NE(suffix_hex, nullptr);
    ASSERT_TRUE(yyjson_is_uint(count_value));
    const std::vector<std::byte> prefix = decode_hex(prefix_hex);
    const std::vector<std::byte> suffix = decode_hex(suffix_hex);
    ASSERT_EQ(prefix.size(), 1U);

    std::vector<std::byte> sample(yyjson_get_uint(count_value), prefix.front());
    sample.insert(sample.end(), suffix.begin(), suffix.end());
    expect_mime(sample, expected);
}

} // namespace

TEST(Mime, MatchesCheckedInSniffAndOverrideVectors) {
    const auto document = load_mime_vectors();
    ASSERT_NE(document, nullptr);
    yyjson_val *const root = yyjson_doc_get_root(document.get());
    ASSERT_TRUE(yyjson_is_obj(root));
    yyjson_val *const cases = yyjson_obj_get(root, "cases");
    ASSERT_TRUE(yyjson_is_arr(cases));

    std::size_t index = 0;
    std::size_t maximum = 0;
    yyjson_val *vector = nullptr;
    yyjson_arr_foreach(cases, index, maximum, vector) {
        ASSERT_TRUE(yyjson_is_obj(vector));
        const char *const id = yyjson_get_str(yyjson_obj_get(vector, "id"));
        const char *const expected = yyjson_get_str(yyjson_obj_get(vector, "expected"));
        ASSERT_NE(id, nullptr);
        ASSERT_NE(expected, nullptr);
        SCOPED_TRACE(id);

        if (yyjson_val *value = yyjson_obj_get(vector, "sampleHex")) {
            const char *text = yyjson_get_str(value);
            ASSERT_NE(text, nullptr);
            expect_mime(decode_hex(text), expected);
        } else if (yyjson_val *value = yyjson_obj_get(vector, "sampleUtf8")) {
            const char *text = yyjson_get_str(value);
            ASSERT_NE(text, nullptr);
            expect_mime(utf8_sample(text), expected);
        } else if (yyjson_val *value = yyjson_obj_get(vector, "sampleHexVariants")) {
            check_string_variants(value, expected, true);
        } else if (yyjson_val *value = yyjson_obj_get(vector, "sampleUtf8Variants")) {
            check_string_variants(value, expected, false);
        } else {
            check_generated_sample(yyjson_obj_get(vector, "generatedSample"), expected);
        }
    }
}
