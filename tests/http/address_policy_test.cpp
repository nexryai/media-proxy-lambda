#include <cstddef>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>

#include <gtest/gtest.h>
#include <mediaproxy/http/address_policy.hpp>
#include <yyjson.h>

namespace {

using mediaproxy::http::AddressError;
using mediaproxy::http::AddressFamily;
using mediaproxy::http::validate_public_address;

std::unique_ptr<yyjson_doc, decltype(&yyjson_doc_free)> LoadAddressVectors()
{
    const std::string path = std::string{MEDIAPROXY_SOURCE_DIR}
        + "/tests/vectors/address-policy.json";
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

TEST(AddressPolicy, MatchesCheckedInPublicAddressCorpus)
{
    const auto document = LoadAddressVectors();
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
        const char* const address =
            yyjson_get_str(yyjson_obj_get(vector, "address"));
        yyjson_val* const accepted_value =
            yyjson_obj_get(vector, "accepted");
        ASSERT_NE(id, nullptr);
        ASSERT_NE(address, nullptr);
        ASSERT_TRUE(yyjson_is_bool(accepted_value));
        SCOPED_TRACE(id);

        const auto result = validate_public_address(address);
        const bool accepted = yyjson_get_bool(accepted_value);
        EXPECT_EQ(static_cast<bool>(result), accepted);
        EXPECT_EQ(result.error == AddressError::none, accepted);
    }
}

TEST(AddressPolicy, PreservesValidatedFamilyAndRejectsParserConfusion)
{
    const auto ipv4 = validate_public_address("1.1.1.1");
    ASSERT_TRUE(ipv4);
    EXPECT_EQ(ipv4.address.family, AddressFamily::ipv4);
    EXPECT_EQ(ipv4.address.bytes[0], 1);
    EXPECT_EQ(ipv4.address.bytes[3], 1);

    const auto ipv6 = validate_public_address("2606:4700:4700::1111");
    ASSERT_TRUE(ipv6);
    EXPECT_EQ(ipv6.address.family, AddressFamily::ipv6);
    EXPECT_EQ(ipv6.address.bytes[0], 0x26);
    EXPECT_EQ(ipv6.address.bytes[1], 0x06);

    const std::string embedded_nul{"1.1.1.1\0.example", 16};
    const auto nul_result = validate_public_address(embedded_nul);
    EXPECT_FALSE(nul_result);
    EXPECT_EQ(nul_result.error, AddressError::parse_failure);

    EXPECT_FALSE(validate_public_address("::FFFF:192.0.2.1"));
    EXPECT_FALSE(validate_public_address("1.1.1.1%eth0"));
    EXPECT_FALSE(validate_public_address("2606:4700:4700::1111%eth0"));
}

TEST(AddressPolicy, ChecksBothSidesOfExplicitRangeBoundaries)
{
    EXPECT_TRUE(validate_public_address("100.63.255.255"));
    EXPECT_FALSE(validate_public_address("100.64.0.0"));
    EXPECT_FALSE(validate_public_address("100.127.255.255"));
    EXPECT_TRUE(validate_public_address("100.128.0.0"));

    EXPECT_FALSE(validate_public_address("2001:db8::"));
    EXPECT_FALSE(validate_public_address("2001:db8:ffff:ffff::"));
    EXPECT_TRUE(validate_public_address("2001:db9::"));
}
