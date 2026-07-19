#include <mediaproxy/http/dns_resolver.hpp>

#include <array>
#include <cstddef>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <mediaproxy/http/address_policy.hpp>
#include <mediaproxy/http/dns_policy.hpp>
#include <mediaproxy/http/url_policy.hpp>

namespace mediaproxy::http {
namespace {

class AddressInfoDeleter final {
public:
    explicit AddressInfoDeleter(AddressReleaseFunction release) noexcept
        : release_(release)
    {
    }

    void operator()(addrinfo* result) const noexcept
    {
        if (result != nullptr && release_ != nullptr) {
            release_(result);
        }
    }

private:
    AddressReleaseFunction release_;
};

using AddressInfo = std::unique_ptr<addrinfo, AddressInfoDeleter>;

[[nodiscard]] bool equal_address(
    const std::optional<ValidatedAddress>& left,
    const std::optional<ValidatedAddress>& right) noexcept
{
    if (left.has_value() != right.has_value()) {
        return false;
    }
    return !left || (left->family == right->family
        && left->bytes == right->bytes);
}

[[nodiscard]] std::optional<OriginUrl> revalidate_origin(
    const OriginUrl& origin)
{
    UrlPolicyResult validated = validate_origin_url(origin.canonical_url);
    if (!validated
        || validated.url->canonical_url != origin.canonical_url
        || validated.url->hostname != origin.hostname
        || validated.url->port != origin.port
        || validated.url->request_target != origin.request_target
        || !equal_address(
            validated.url->literal_address, origin.literal_address)) {
        return std::nullopt;
    }
    return std::move(validated.url);
}

[[nodiscard]] OriginResolutionResult fail(
    OriginResolutionError error,
    int native_error = 0)
{
    return {
        .addresses = {},
        .error = error,
        .native_error = native_error,
        .policy_error = ResolutionError::none,
        .rejected_index = std::nullopt,
        .address_error = AddressError::none,
    };
}

[[nodiscard]] bool format_candidate(
    const addrinfo& candidate,
    std::span<char> output) noexcept
{
    if (candidate.ai_addr == nullptr) {
        return false;
    }

    const void* bytes = nullptr;
    if (candidate.ai_family == AF_INET) {
        if (candidate.ai_addrlen < sizeof(sockaddr_in)
            || candidate.ai_addr->sa_family != AF_INET) {
            return false;
        }
        const auto* address =
            reinterpret_cast<const sockaddr_in*>(candidate.ai_addr);
        bytes = &address->sin_addr;
    } else if (candidate.ai_family == AF_INET6) {
        if (candidate.ai_addrlen < sizeof(sockaddr_in6)
            || candidate.ai_addr->sa_family != AF_INET6) {
            return false;
        }
        const auto* address =
            reinterpret_cast<const sockaddr_in6*>(candidate.ai_addr);
        bytes = &address->sin6_addr;
    } else {
        return false;
    }

    return inet_ntop(
        candidate.ai_family,
        bytes,
        output.data(),
        static_cast<socklen_t>(output.size())) != nullptr;
}

} // namespace

OriginResolutionResult resolve_origin_addresses(
    const OriginUrl& origin,
    AddressResolverApi api)
{
    const std::optional<OriginUrl> validated = revalidate_origin(origin);
    if (!validated) {
        return fail(OriginResolutionError::invalid_origin);
    }
    if (validated->literal_address) {
        return {
            .addresses = {*validated->literal_address},
            .error = OriginResolutionError::none,
            .native_error = 0,
            .policy_error = ResolutionError::none,
            .rejected_index = std::nullopt,
            .address_error = AddressError::none,
        };
    }
    if (api.lookup == nullptr || api.release == nullptr) {
        return fail(OriginResolutionError::lookup_failure, EAI_SYSTEM);
    }

    const addrinfo hints{
        .ai_flags = AI_NUMERICSERV,
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_addrlen = 0,
        .ai_addr = nullptr,
        .ai_canonname = nullptr,
        .ai_next = nullptr,
    };
    const std::string service = std::to_string(validated->port);
    addrinfo* raw = nullptr;
    const int lookup_result = api.lookup(
        validated->hostname.c_str(), service.c_str(), &hints, &raw);
    AddressInfo results{raw, AddressInfoDeleter{api.release}};
    if (lookup_result != 0) {
        return fail(OriginResolutionError::lookup_failure, lookup_result);
    }
    if (!results) {
        return fail(OriginResolutionError::empty_answer);
    }

    std::array<std::array<char, INET6_ADDRSTRLEN>, maximum_dns_candidates>
        text_storage{};
    std::array<std::string_view, maximum_dns_candidates> candidates{};
    std::size_t count = 0;
    for (const addrinfo* current = results.get(); current != nullptr;
         current = current->ai_next) {
        if (count == maximum_dns_candidates) {
            return fail(OriginResolutionError::too_many_answers);
        }
        if (!format_candidate(*current, text_storage[count])) {
            return fail(OriginResolutionError::malformed_answer);
        }
        candidates[count] = text_storage[count].data();
        ++count;
    }
    if (count == 0) {
        return fail(OriginResolutionError::empty_answer);
    }

    ResolutionPolicyResult policy = validate_resolved_addresses(
        std::span<const std::string_view>{candidates}.first(count));
    if (!policy) {
        return {
            .addresses = {},
            .error = OriginResolutionError::address_policy,
            .native_error = 0,
            .policy_error = policy.error,
            .rejected_index = policy.rejected_index,
            .address_error = policy.address_error,
        };
    }
    return {
        .addresses = std::move(policy.addresses),
        .error = OriginResolutionError::none,
        .native_error = 0,
        .policy_error = ResolutionError::none,
        .rejected_index = std::nullopt,
        .address_error = AddressError::none,
    };
}

} // namespace mediaproxy::http
