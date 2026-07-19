#include <array>
#include <cstddef>
#include <string>

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/dns_policy.hpp>
#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace {

using mediaproxy::http::AddressError;
using mediaproxy::http::AddressFamily;
using mediaproxy::http::AddressResolverApi;
using mediaproxy::http::OriginResolutionError;
using mediaproxy::http::ResolutionError;
using mediaproxy::http::maximum_dns_candidates;
using mediaproxy::http::resolve_origin_addresses;
using mediaproxy::http::validate_origin_url;

struct FakeResolverState {
    addrinfo* result = nullptr;
    int error = 0;
    int lookup_calls = 0;
    int release_calls = 0;
    std::string hostname;
    std::string service;
    addrinfo hints{};
};

FakeResolverState* active_state = nullptr;

int FakeLookup(
    const char* hostname,
    const char* service,
    const addrinfo* hints,
    addrinfo** result)
{
    if (active_state == nullptr || hostname == nullptr || service == nullptr
        || hints == nullptr || result == nullptr) {
        return EAI_FAIL;
    }
    ++active_state->lookup_calls;
    active_state->hostname = hostname;
    active_state->service = service;
    active_state->hints = *hints;
    *result = active_state->result;
    return active_state->error;
}

void FakeRelease(addrinfo*)
{
    if (active_state != nullptr) {
        ++active_state->release_calls;
    }
}

class FakeResolverScope final {
public:
    explicit FakeResolverScope(FakeResolverState& state)
    {
        EXPECT_EQ(active_state, nullptr);
        active_state = &state;
    }

    ~FakeResolverScope()
    {
        active_state = nullptr;
    }

    FakeResolverScope(const FakeResolverScope&) = delete;
    FakeResolverScope& operator=(const FakeResolverScope&) = delete;
};

struct Candidate {
    sockaddr_storage storage{};
    addrinfo info{};
};

void SetIpv4(Candidate& candidate, const char* text)
{
    candidate = {};
    auto* address = reinterpret_cast<sockaddr_in*>(&candidate.storage);
    address->sin_family = AF_INET;
    EXPECT_EQ(inet_pton(AF_INET, text, &address->sin_addr), 1);
    candidate.info.ai_family = AF_INET;
    candidate.info.ai_socktype = SOCK_STREAM;
    candidate.info.ai_protocol = IPPROTO_TCP;
    candidate.info.ai_addrlen = sizeof(sockaddr_in);
    candidate.info.ai_addr = reinterpret_cast<sockaddr*>(address);
}

void SetIpv6(Candidate& candidate, const char* text)
{
    candidate = {};
    auto* address = reinterpret_cast<sockaddr_in6*>(&candidate.storage);
    address->sin6_family = AF_INET6;
    EXPECT_EQ(inet_pton(AF_INET6, text, &address->sin6_addr), 1);
    candidate.info.ai_family = AF_INET6;
    candidate.info.ai_socktype = SOCK_STREAM;
    candidate.info.ai_protocol = IPPROTO_TCP;
    candidate.info.ai_addrlen = sizeof(sockaddr_in6);
    candidate.info.ai_addr = reinterpret_cast<sockaddr*>(address);
}

AddressResolverApi FakeApi()
{
    return {
        .lookup = &FakeLookup,
        .release = &FakeRelease,
    };
}

TEST(DnsResolver, ResolvesCanonicalHostOnceAndPreservesAnswerOrder)
{
    Candidate first;
    Candidate second;
    Candidate third;
    SetIpv4(first, "1.1.1.1");
    SetIpv6(second, "2606:4700:4700::1111");
    SetIpv4(third, "8.8.8.8");
    first.info.ai_next = &second.info;
    second.info.ai_next = &third.info;
    FakeResolverState state;
    state.result = &first.info;
    const FakeResolverScope scope{state};

    const auto origin =
        validate_origin_url("https://bücher.example:80/image");
    ASSERT_TRUE(origin);
    const auto result = resolve_origin_addresses(*origin.url, FakeApi());
    ASSERT_TRUE(result);
    ASSERT_EQ(result.addresses.size(), 3U);
    EXPECT_EQ(result.addresses[0].family, AddressFamily::ipv4);
    EXPECT_EQ(result.addresses[1].family, AddressFamily::ipv6);
    EXPECT_EQ(result.addresses[2].family, AddressFamily::ipv4);
    EXPECT_EQ(state.lookup_calls, 1);
    EXPECT_EQ(state.release_calls, 1);
    EXPECT_EQ(state.hostname, "xn--bcher-kva.example");
    EXPECT_EQ(state.service, "80");
    EXPECT_EQ(state.hints.ai_family, AF_UNSPEC);
    EXPECT_EQ(state.hints.ai_socktype, SOCK_STREAM);
    EXPECT_EQ(state.hints.ai_protocol, IPPROTO_TCP);
    EXPECT_NE(state.hints.ai_flags & AI_NUMERICSERV, 0);
}

TEST(DnsResolver, RejectsWholeMixedAnswerAndReleasesIt)
{
    Candidate public_address;
    Candidate private_address;
    SetIpv4(public_address, "1.1.1.1");
    SetIpv4(private_address, "192.168.1.1");
    public_address.info.ai_next = &private_address.info;
    FakeResolverState state;
    state.result = &public_address.info;
    const FakeResolverScope scope{state};

    const auto origin = validate_origin_url("https://origin.example/");
    ASSERT_TRUE(origin);
    const auto result = resolve_origin_addresses(*origin.url, FakeApi());
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginResolutionError::address_policy);
    EXPECT_EQ(result.policy_error, ResolutionError::forbidden_address);
    EXPECT_EQ(result.address_error, AddressError::private_address);
    ASSERT_TRUE(result.rejected_index.has_value());
    EXPECT_EQ(*result.rejected_index, 1U);
    EXPECT_TRUE(result.addresses.empty());
    EXPECT_EQ(state.release_calls, 1);
}

TEST(DnsResolver, ReportsLookupEmptyAndMalformedAnswers)
{
    const auto origin = validate_origin_url("https://origin.example/");
    ASSERT_TRUE(origin);

    Candidate unexpected_failure_result;
    SetIpv4(unexpected_failure_result, "1.1.1.1");
    FakeResolverState failed;
    failed.result = &unexpected_failure_result.info;
    failed.error = EAI_AGAIN;
    {
        const FakeResolverScope scope{failed};
        const auto result = resolve_origin_addresses(*origin.url, FakeApi());
        EXPECT_FALSE(result);
        EXPECT_EQ(result.error, OriginResolutionError::lookup_failure);
        EXPECT_EQ(result.native_error, EAI_AGAIN);
        EXPECT_EQ(failed.release_calls, 1);
    }

    FakeResolverState empty;
    {
        const FakeResolverScope scope{empty};
        const auto result = resolve_origin_addresses(*origin.url, FakeApi());
        EXPECT_FALSE(result);
        EXPECT_EQ(result.error, OriginResolutionError::empty_answer);
    }

    Candidate malformed;
    SetIpv4(malformed, "1.1.1.1");
    malformed.info.ai_addrlen = sizeof(sockaddr_in) - 1;
    FakeResolverState malformed_state;
    malformed_state.result = &malformed.info;
    {
        const FakeResolverScope scope{malformed_state};
        const auto result = resolve_origin_addresses(*origin.url, FakeApi());
        EXPECT_FALSE(result);
        EXPECT_EQ(result.error, OriginResolutionError::malformed_answer);
        EXPECT_EQ(malformed_state.release_calls, 1);
    }
}

TEST(DnsResolver, BoundsCandidateTraversal)
{
    std::array<Candidate, maximum_dns_candidates + 1> candidates{};
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        SetIpv4(candidates[index], "1.1.1.1");
        if (index + 1 < candidates.size()) {
            candidates[index].info.ai_next = &candidates[index + 1].info;
        }
    }
    FakeResolverState state;
    state.result = &candidates[0].info;
    const FakeResolverScope scope{state};
    const auto origin = validate_origin_url("https://origin.example/");
    ASSERT_TRUE(origin);

    const auto result = resolve_origin_addresses(*origin.url, FakeApi());
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginResolutionError::too_many_answers);
    EXPECT_TRUE(result.addresses.empty());
    EXPECT_EQ(state.release_calls, 1);
}

TEST(DnsResolver, BypassesLookupForValidatedLiteralAndRejectsForgery)
{
    FakeResolverState state;
    state.error = EAI_FAIL;
    const FakeResolverScope scope{state};
    const auto literal = validate_origin_url("https://1.1.1.1/image");
    ASSERT_TRUE(literal);

    const auto result = resolve_origin_addresses(*literal.url, FakeApi());
    ASSERT_TRUE(result);
    ASSERT_EQ(result.addresses.size(), 1U);
    EXPECT_EQ(result.addresses[0].family, AddressFamily::ipv4);
    EXPECT_EQ(state.lookup_calls, 0);
    EXPECT_EQ(state.release_calls, 0);

    auto forged = *literal.url;
    forged.hostname = "8.8.8.8";
    const auto rejected = resolve_origin_addresses(forged, FakeApi());
    EXPECT_FALSE(rejected);
    EXPECT_EQ(rejected.error, OriginResolutionError::invalid_origin);
    EXPECT_EQ(state.lookup_calls, 0);
}

} // namespace
