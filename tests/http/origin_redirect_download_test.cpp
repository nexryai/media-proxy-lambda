#include <array>
#include <cstddef>
#include <string>
#include <string_view>

#include <arpa/inet.h>
#include <curl/curl.h>
#include <gtest/gtest.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/origin_download.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/redirect_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace {

using mediaproxy::http::AddressResolverApi;
using mediaproxy::http::OriginDownloadError;
using mediaproxy::http::OriginResponseAccumulator;
using mediaproxy::http::OriginResponseError;
using mediaproxy::http::OriginTimeoutApi;
using mediaproxy::http::OriginTransportApi;
using mediaproxy::http::RedirectError;
using mediaproxy::http::UrlError;
using mediaproxy::http::download_origin;
using mediaproxy::http::maximum_origin_redirects;
using mediaproxy::http::validate_origin_url;

class CurlGlobal final {
public:
    CurlGlobal() noexcept
        : result_(curl_global_init(CURL_GLOBAL_DEFAULT))
    {
    }

    ~CurlGlobal()
    {
        if (result_ == CURLE_OK) {
            curl_global_cleanup();
        }
    }

    [[nodiscard]] CURLcode result() const noexcept
    {
        return result_;
    }

private:
    CURLcode result_;
};

inline constexpr std::size_t maximum_test_hops =
    maximum_origin_redirects + 2;

struct RedirectTransportState {
    std::array<long, maximum_test_hops> statuses{};
    std::array<std::string_view, maximum_test_hops> header_lines{};
    std::array<std::string, maximum_test_hops> effective_urls{};
    std::size_t hop_count = 0;
    std::size_t create_calls = 0;
    std::size_t destroy_calls = 0;
    std::size_t perform_calls = 0;
    std::size_t response_code_calls = 0;
};

CURL* RedirectCreate(void* context)
{
    auto& state = *static_cast<RedirectTransportState*>(context);
    ++state.create_calls;
    return curl_easy_init();
}

void RedirectDestroy(CURL* easy, void* context)
{
    auto& state = *static_cast<RedirectTransportState*>(context);
    ++state.destroy_calls;
    curl_easy_cleanup(easy);
}

CURLcode RedirectPerform(
    CURL* easy,
    OriginResponseAccumulator& response,
    void* context)
{
    auto& state = *static_cast<RedirectTransportState*>(context);
    const std::size_t index = state.perform_calls++;
    if (index >= state.hop_count) {
        return CURLE_FAILED_INIT;
    }

    char* effective_url = nullptr;
    if (curl_easy_getinfo(easy, CURLINFO_EFFECTIVE_URL, &effective_url)
            == CURLE_OK
        && effective_url != nullptr) {
        state.effective_urls[index] = effective_url;
    }
    if (!state.header_lines[index].empty()) {
        response.consume_header_line(state.header_lines[index]);
    }
    return CURLE_OK;
}

CURLcode RedirectResponseCode(CURL*, long* status, void* context)
{
    auto& state = *static_cast<RedirectTransportState*>(context);
    const std::size_t index = state.response_code_calls++;
    if (index >= state.hop_count) {
        return CURLE_BAD_FUNCTION_ARGUMENT;
    }
    *status = state.statuses[index];
    return CURLE_OK;
}

OriginTransportApi RedirectTransport(RedirectTransportState& state)
{
    return {
        .context = &state,
        .create = &RedirectCreate,
        .destroy = &RedirectDestroy,
        .perform = &RedirectPerform,
        .response_code = &RedirectResponseCode,
    };
}

struct TimeoutState {
    long milliseconds = 1500;
    std::size_t calls = 0;
    std::size_t expire_on_call = static_cast<std::size_t>(-1);
};

long RemainingTime(void* context)
{
    auto& state = *static_cast<TimeoutState*>(context);
    const std::size_t call = state.calls++;
    return call == state.expire_on_call ? 0 : state.milliseconds;
}

OriginTimeoutApi Timeout(TimeoutState& state)
{
    return {
        .context = &state,
        .remaining_milliseconds = &RemainingTime,
    };
}

struct ResolverState {
    sockaddr_in address{};
    addrinfo answer{};
    std::array<std::string, 2> hostnames{};
    std::size_t lookup_calls = 0;
    std::size_t release_calls = 0;
};

ResolverState* active_resolver = nullptr;

int CountingLookup(
    const char* hostname,
    const char*,
    const addrinfo*,
    addrinfo** result)
{
    if (active_resolver == nullptr || hostname == nullptr || result == nullptr
        || active_resolver->lookup_calls
            >= active_resolver->hostnames.size()) {
        return EAI_FAIL;
    }
    active_resolver->hostnames[active_resolver->lookup_calls] = hostname;
    ++active_resolver->lookup_calls;
    *result = &active_resolver->answer;
    return 0;
}

void CountingRelease(addrinfo*)
{
    if (active_resolver != nullptr) {
        ++active_resolver->release_calls;
    }
}

class ResolverScope final {
public:
    explicit ResolverScope(ResolverState& state)
    {
        EXPECT_EQ(active_resolver, nullptr);
        state.address.sin_family = AF_INET;
        EXPECT_EQ(
            inet_pton(AF_INET, "1.1.1.1", &state.address.sin_addr),
            1);
        state.answer.ai_family = AF_INET;
        state.answer.ai_socktype = SOCK_STREAM;
        state.answer.ai_protocol = IPPROTO_TCP;
        state.answer.ai_addrlen = sizeof(state.address);
        state.answer.ai_addr = reinterpret_cast<sockaddr*>(&state.address);
        active_resolver = &state;
    }

    ~ResolverScope()
    {
        active_resolver = nullptr;
    }

    ResolverScope(const ResolverScope&) = delete;
    ResolverScope& operator=(const ResolverScope&) = delete;
};

AddressResolverApi CountingResolver()
{
    return {
        .lookup = &CountingLookup,
        .release = &CountingRelease,
    };
}

TEST(OriginRedirectDownload, FollowsEachSupportedStatusWithFreshHop)
{
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    const auto origin = validate_origin_url("https://1.1.1.1/start");
    ASSERT_TRUE(origin);

    for (const long redirect_status : std::array{301L, 302L, 303L, 307L, 308L}) {
        RedirectTransportState transport;
        transport.hop_count = 2;
        transport.statuses[0] = redirect_status;
        transport.statuses[1] = 200;
        transport.header_lines[0] = "Location: /next?value=1\r\n";
        TimeoutState timeout;

        const auto result = download_origin(
            *origin.url, Timeout(timeout), {}, RedirectTransport(transport));

        ASSERT_TRUE(result) << redirect_status;
        EXPECT_EQ(result.redirect_count, 1U);
        EXPECT_EQ(transport.perform_calls, 2U);
        EXPECT_EQ(transport.create_calls, 2U);
        EXPECT_EQ(transport.destroy_calls, 2U);
        EXPECT_EQ(timeout.calls, 4U);
        EXPECT_EQ(transport.effective_urls[0], "https://1.1.1.1/start");
        EXPECT_EQ(
            transport.effective_urls[1],
            "https://1.1.1.1/next?value=1");
    }
}

TEST(OriginRedirectDownload, RejectsMissingUnsafeAndUnsupportedRedirects)
{
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    const auto origin = validate_origin_url("https://1.1.1.1/start");
    ASSERT_TRUE(origin);

    RedirectTransportState missing;
    missing.hop_count = 1;
    missing.statuses[0] = 302;
    TimeoutState missing_timeout;
    auto result = download_origin(
        *origin.url,
        Timeout(missing_timeout),
        {},
        RedirectTransport(missing));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::redirect);
    EXPECT_EQ(result.redirect_error, RedirectError::invalid_location);

    RedirectTransportState unsafe;
    unsafe.hop_count = 1;
    unsafe.statuses[0] = 302;
    unsafe.header_lines[0] = "Location: https://127.0.0.1/private\r\n";
    TimeoutState unsafe_timeout;
    result = download_origin(
        *origin.url,
        Timeout(unsafe_timeout),
        {},
        RedirectTransport(unsafe));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::redirect);
    EXPECT_EQ(result.redirect_error, RedirectError::url_policy);
    EXPECT_EQ(result.redirect_url_error, UrlError::forbidden_address);
    EXPECT_EQ(unsafe.perform_calls, 1U);

    RedirectTransportState unsupported;
    unsupported.hop_count = 1;
    unsupported.statuses[0] = 300;
    unsupported.header_lines[0] = "Location: /ignored\r\n";
    TimeoutState unsupported_timeout;
    result = download_origin(
        *origin.url,
        Timeout(unsupported_timeout),
        {},
        RedirectTransport(unsupported));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::response_policy);
    EXPECT_EQ(result.response.error(), OriginResponseError::non_200_status);
    EXPECT_EQ(unsupported.perform_calls, 1U);

    RedirectTransportState loop;
    loop.hop_count = 2;
    loop.statuses[0] = 302;
    loop.statuses[1] = 302;
    loop.header_lines[0] = "Location: /next\r\n";
    loop.header_lines[1] = "Location: /start\r\n";
    TimeoutState loop_timeout;
    result = download_origin(
        *origin.url,
        Timeout(loop_timeout),
        {},
        RedirectTransport(loop));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::redirect);
    EXPECT_EQ(result.redirect_error, RedirectError::loop);
    EXPECT_EQ(loop.perform_calls, 2U);
}

TEST(OriginRedirectDownload, ResolvesAndPinsEveryHostnameHop)
{
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    const auto origin = validate_origin_url("https://first.example/start");
    ASSERT_TRUE(origin);
    ResolverState resolver_state;
    const ResolverScope resolver_scope{resolver_state};
    RedirectTransportState transport;
    transport.hop_count = 2;
    transport.statuses[0] = 302;
    transport.statuses[1] = 200;
    transport.header_lines[0] =
        "Location: https://second.example/final\r\n";
    TimeoutState timeout;

    const auto result = download_origin(
        *origin.url,
        Timeout(timeout),
        CountingResolver(),
        RedirectTransport(transport));

    ASSERT_TRUE(result);
    EXPECT_EQ(result.redirect_count, 1U);
    EXPECT_EQ(resolver_state.lookup_calls, 2U);
    EXPECT_EQ(resolver_state.release_calls, 2U);
    EXPECT_EQ(resolver_state.hostnames[0], "first.example");
    EXPECT_EQ(resolver_state.hostnames[1], "second.example");
    EXPECT_EQ(transport.effective_urls[0], "https://first.example/start");
    EXPECT_EQ(transport.effective_urls[1], "https://second.example/final");
}

TEST(OriginRedirectDownload, RejectsTheEleventhRedirectResponse)
{
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    const auto origin = validate_origin_url("https://1.1.1.1/start");
    ASSERT_TRUE(origin);
    constexpr std::array<std::string_view, maximum_origin_redirects + 1>
        headers{
            "Location: /1\r\n", "Location: /2\r\n",
            "Location: /3\r\n", "Location: /4\r\n",
            "Location: /5\r\n", "Location: /6\r\n",
            "Location: /7\r\n", "Location: /8\r\n",
            "Location: /9\r\n", "Location: /10\r\n",
            "Location: /11\r\n",
        };
    RedirectTransportState transport;
    transport.hop_count = headers.size();
    TimeoutState timeout;
    for (std::size_t index = 0; index < headers.size(); ++index) {
        transport.statuses[index] = 302;
        transport.header_lines[index] = headers[index];
    }

    const auto result = download_origin(
        *origin.url, Timeout(timeout), {}, RedirectTransport(transport));

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::redirect);
    EXPECT_EQ(result.redirect_error, RedirectError::too_many_redirects);
    EXPECT_EQ(result.redirect_count, maximum_origin_redirects);
    EXPECT_EQ(transport.perform_calls, maximum_origin_redirects + 1);
}

TEST(OriginRedirectDownload, RefreshesAndEnforcesRemainingTime)
{
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    const auto origin = validate_origin_url("https://1.1.1.1/start");
    ASSERT_TRUE(origin);

    RedirectTransportState after_dns;
    after_dns.hop_count = 1;
    after_dns.statuses[0] = 200;
    TimeoutState after_dns_timeout;
    after_dns_timeout.expire_on_call = 1;
    auto result = download_origin(
        *origin.url,
        Timeout(after_dns_timeout),
        {},
        RedirectTransport(after_dns));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::deadline);
    EXPECT_EQ(after_dns.create_calls, 0U);
    EXPECT_EQ(after_dns_timeout.calls, 2U);

    RedirectTransportState next_hop;
    next_hop.hop_count = 2;
    next_hop.statuses[0] = 302;
    next_hop.statuses[1] = 200;
    next_hop.header_lines[0] = "Location: /next\r\n";
    TimeoutState next_hop_timeout;
    next_hop_timeout.expire_on_call = 2;
    result = download_origin(
        *origin.url,
        Timeout(next_hop_timeout),
        {},
        RedirectTransport(next_hop));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::deadline);
    EXPECT_EQ(result.redirect_count, 1U);
    EXPECT_EQ(next_hop.perform_calls, 1U);
    EXPECT_EQ(next_hop_timeout.calls, 3U);

    RedirectTransportState invalid_timeout;
    invalid_timeout.hop_count = 1;
    result = download_origin(
        *origin.url, {}, {}, RedirectTransport(invalid_timeout));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::invalid_argument);
    EXPECT_EQ(invalid_timeout.create_calls, 0U);
}

} // namespace
