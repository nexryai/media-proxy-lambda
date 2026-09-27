#include <algorithm>
#include <array>
#include <cstddef>
#include <span>

#include <curl/curl.h>
#include <gtest/gtest.h>
#include <mediaproxy/http/dns_resolver.hpp>
#include <mediaproxy/http/origin_download.hpp>
#include <mediaproxy/http/origin_response.hpp>
#include <mediaproxy/http/url_policy.hpp>
#include <netdb.h>

namespace {

using mediaproxy::http::AddressResolverApi;
using mediaproxy::http::download_origin_once;
using mediaproxy::http::maximum_origin_body_bytes;
using mediaproxy::http::OriginDownloadError;
using mediaproxy::http::OriginResponseAccumulator;
using mediaproxy::http::OriginResponseError;
using mediaproxy::http::OriginTransportApi;
using mediaproxy::http::system_origin_transport;
using mediaproxy::http::validate_origin_url;

class CurlGlobal final {
  public:
    CurlGlobal() noexcept : result_(curl_global_init(CURL_GLOBAL_DEFAULT)) {}

    ~CurlGlobal() {
        if (result_ == CURLE_OK) {
            curl_global_cleanup();
        }
    }

    [[nodiscard]] auto result() const noexcept -> CURLcode {

        return result_;
    }

  private:
    CURLcode result_;
};

enum class ResponseAction {
    none,
    fill_body_limit,
    block_by_nextdns,
};

struct FakeTransportState {
    CURLcode perform_result = CURLE_OK;
    CURLcode response_code_result = CURLE_OK;
    long status = 200;
    ResponseAction action = ResponseAction::none;
    bool fail_create = false;
    int create_calls = 0;
    int destroy_calls = 0;
    int perform_calls = 0;
    int response_code_calls = 0;
};

auto fake_create(void *context) -> CURL * {
    auto &state = *static_cast<FakeTransportState *>(context);
    ++state.create_calls;

    return state.fail_create ? nullptr : curl_easy_init();
}

void fake_destroy(CURL *easy, void *context) {
    auto &state = *static_cast<FakeTransportState *>(context);
    ++state.destroy_calls;
    curl_easy_cleanup(easy);
}

auto fake_perform(CURL * /*unused*/, OriginResponseAccumulator &response, void *context) -> CURLcode {
    auto &state = *static_cast<FakeTransportState *>(context);
    ++state.perform_calls;
    if (state.action == ResponseAction::fill_body_limit) {
        const std::array<std::byte, 64U * 1024U> chunk{};
        while (!response.at_body_limit()) {
            const std::size_t remaining = maximum_origin_body_bytes - response.body().size();
            EXPECT_EQ(response.append_body(std::span{chunk}.first(std::min(remaining, chunk.size()))), std::min(remaining, chunk.size()));
        }
    } else if (state.action == ResponseAction::block_by_nextdns) {
        response.consume_header_line("Blocked-By: NextDNS\r\n");
    }

    return state.perform_result;
}

auto fake_response_code(CURL * /*unused*/, long *status, void *context) -> CURLcode {
    auto &state = *static_cast<FakeTransportState *>(context);
    ++state.response_code_calls;

    *status = state.status;
    return state.response_code_result;
}

auto fake_transport(FakeTransportState &state) -> OriginTransportApi {

    return {
        .context = &state,
        .create = &fake_create,
        .destroy = &fake_destroy,
        .perform = &fake_perform,
        .response_code = &fake_response_code,
    };
}

auto failing_lookup(const char * /*unused*/, const char * /*unused*/, const addrinfo * /*unused*/, addrinfo **result) -> int {

    *result = nullptr;
    return EAI_AGAIN;
}

void noop_release(addrinfo * /*unused*/) {}

TEST(OriginDownload, PerformsValidatedPinnedLiteralRequest) {
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    const auto origin = validate_origin_url("https://1.1.1.1/image");
    ASSERT_TRUE(origin);
    FakeTransportState transport_state;

    const auto result = download_origin_once(*origin.url, 1500, {}, fake_transport(transport_state));

    EXPECT_TRUE(result);
    EXPECT_EQ(result.status, 200);
    EXPECT_EQ(result.response.error(), OriginResponseError::none);
    ASSERT_EQ(result.resolution.addresses.size(), 1U);
    EXPECT_EQ(transport_state.create_calls, 1);
    EXPECT_EQ(transport_state.perform_calls, 1);
    EXPECT_EQ(transport_state.response_code_calls, 1);
    EXPECT_EQ(transport_state.destroy_calls, 1);
}

TEST(OriginDownload, StopsBeforeTransportWhenResolutionFails) {
    const auto origin = validate_origin_url("https://origin.example/image");
    ASSERT_TRUE(origin);
    FakeTransportState transport_state;
    const AddressResolverApi resolver{
        .lookup = &failing_lookup,
        .release = &noop_release,
    };

    const auto result = download_origin_once(*origin.url, 1500, resolver, fake_transport(transport_state));

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::resolution);
    EXPECT_EQ(result.resolution.native_error, EAI_AGAIN);
    EXPECT_EQ(transport_state.create_calls, 0);
}

TEST(OriginDownload, SeparatesTransferInfoAndResponsePolicyFailures) {
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    const auto origin = validate_origin_url("https://1.1.1.1/image");
    ASSERT_TRUE(origin);

    FakeTransportState transfer;
    transfer.perform_result = CURLE_COULDNT_CONNECT;
    auto result = download_origin_once(*origin.url, 1500, {}, fake_transport(transfer));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::transfer);
    EXPECT_EQ(transfer.response_code_calls, 0);
    EXPECT_EQ(transfer.destroy_calls, 1);

    FakeTransportState response_info;
    response_info.response_code_result = CURLE_BAD_FUNCTION_ARGUMENT;
    result = download_origin_once(*origin.url, 1500, {}, fake_transport(response_info));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::response_info);
    EXPECT_EQ(response_info.destroy_calls, 1);

    FakeTransportState non_200;
    non_200.status = 404;
    result = download_origin_once(*origin.url, 1500, {}, fake_transport(non_200));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::response_policy);
    EXPECT_EQ(result.response.error(), OriginResponseError::non_200_status);
}

TEST(OriginDownload, PreservesExactLimitAndCallbackPolicyErrors) {
    const CurlGlobal global;
    ASSERT_EQ(global.result(), CURLE_OK);
    const auto origin = validate_origin_url("https://1.1.1.1/image");
    ASSERT_TRUE(origin);

    FakeTransportState exact_limit;
    exact_limit.perform_result = CURLE_WRITE_ERROR;
    exact_limit.action = ResponseAction::fill_body_limit;
    auto result = download_origin_once(*origin.url, 1500, {}, fake_transport(exact_limit));
    EXPECT_TRUE(result);
    EXPECT_EQ(result.response.body().size(), maximum_origin_body_bytes);

    FakeTransportState blocked;
    blocked.perform_result = CURLE_WRITE_ERROR;
    blocked.action = ResponseAction::block_by_nextdns;
    result = download_origin_once(*origin.url, 1500, {}, fake_transport(blocked));
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::response_policy);
    EXPECT_EQ(result.response.error(), OriginResponseError::blocked_by_nextdns);
    EXPECT_EQ(blocked.response_code_calls, 0);
}

TEST(OriginDownload, RejectsInvalidTimeoutAndTransportTable) {
    const auto origin = validate_origin_url("https://1.1.1.1/image");
    ASSERT_TRUE(origin);
    FakeTransportState transport_state;
    auto transport = fake_transport(transport_state);

    auto result = download_origin_once(*origin.url, 0, {}, transport);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::invalid_argument);
    EXPECT_EQ(transport_state.create_calls, 0);

    transport.perform = nullptr;
    result = download_origin_once(*origin.url, 1500, {}, transport);
    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::invalid_argument);
    EXPECT_EQ(transport_state.create_calls, 0);

    const auto system = system_origin_transport();
    EXPECT_NE(system.create, nullptr);
    EXPECT_NE(system.destroy, nullptr);
    EXPECT_NE(system.perform, nullptr);
    EXPECT_NE(system.response_code, nullptr);
}

TEST(OriginDownload, ReportsEasyHandleAllocationFailure) {
    const auto origin = validate_origin_url("https://1.1.1.1/image");
    ASSERT_TRUE(origin);
    FakeTransportState transport_state;
    transport_state.fail_create = true;

    const auto result = download_origin_once(*origin.url, 1500, {}, fake_transport(transport_state));

    EXPECT_FALSE(result);
    EXPECT_EQ(result.error, OriginDownloadError::easy_init);
    EXPECT_EQ(transport_state.create_calls, 1);
    EXPECT_EQ(transport_state.destroy_calls, 0);
    EXPECT_EQ(transport_state.perform_calls, 0);
}

} // namespace
