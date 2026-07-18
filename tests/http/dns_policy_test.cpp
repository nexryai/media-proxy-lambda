#include <array>
#include <string_view>

#include <gtest/gtest.h>
#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/dns_policy.hpp>

namespace {

using mediaproxy::http::AddressError;
using mediaproxy::http::AddressFamily;
using mediaproxy::http::ResolutionError;
using mediaproxy::http::validate_resolved_addresses;

TEST(DnsPolicy, AcceptsEveryPublicCandidateInResolverOrder)
{
    constexpr std::array<std::string_view, 3> candidates{
        "1.1.1.1",
        "2606:4700:4700::1111",
        "8.8.8.8",
    };
    const auto result = validate_resolved_addresses(candidates);
    ASSERT_TRUE(result);
    ASSERT_EQ(result.addresses.size(), candidates.size());
    EXPECT_EQ(result.addresses[0].family, AddressFamily::ipv4);
    EXPECT_EQ(result.addresses[1].family, AddressFamily::ipv6);
    EXPECT_EQ(result.addresses[2].family, AddressFamily::ipv4);
    EXPECT_FALSE(result.rejected_index.has_value());
}

TEST(DnsPolicy, RejectsWholeAnswerWhenAnyCandidateIsForbidden)
{
    constexpr std::array<std::string_view, 3> candidates{
        "1.1.1.1",
        "192.168.1.1",
        "8.8.8.8",
    };
    const auto result = validate_resolved_addresses(candidates);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, ResolutionError::forbidden_address);
    ASSERT_TRUE(result.rejected_index.has_value());
    EXPECT_EQ(*result.rejected_index, 1U);
    EXPECT_EQ(result.address_error, AddressError::private_address);
    EXPECT_TRUE(result.addresses.empty());
}

TEST(DnsPolicy, RejectsEmptyMalformedAndMappedAnswers)
{
    constexpr std::array<std::string_view, 0> empty{};
    const auto empty_result = validate_resolved_addresses(empty);
    EXPECT_FALSE(empty_result);
    EXPECT_EQ(empty_result.error, ResolutionError::empty_answer);

    constexpr std::array<std::string_view, 1> malformed{"not-an-address"};
    const auto malformed_result = validate_resolved_addresses(malformed);
    EXPECT_FALSE(malformed_result);
    EXPECT_EQ(malformed_result.address_error, AddressError::parse_failure);

    constexpr std::array<std::string_view, 1> mapped{"::ffff:1.1.1.1"};
    const auto mapped_result = validate_resolved_addresses(mapped);
    EXPECT_FALSE(mapped_result);
    EXPECT_EQ(mapped_result.address_error, AddressError::mapped_ipv6);
}

} // namespace
