#include <mediaproxy/http/dns_policy.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

#include <mediaproxy/http/address_policy.hpp>

namespace mediaproxy::http {

ResolutionPolicyResult validate_resolved_addresses(
    std::span<const std::string_view> candidates)
{
    if (candidates.empty()) {
        return {
            .addresses = {},
            .error = ResolutionError::empty_answer,
            .rejected_index = std::nullopt,
            .address_error = AddressError::parse_failure,
        };
    }

    std::vector<ValidatedAddress> validated;
    validated.reserve(candidates.size());
    for (std::size_t index = 0; index < candidates.size(); ++index) {
        const auto result = validate_public_address(candidates[index]);
        if (!result) {
            return {
                .addresses = {},
                .error = ResolutionError::forbidden_address,
                .rejected_index = index,
                .address_error = result.error,
            };
        }
        validated.push_back(result.address);
    }
    return {
        .addresses = std::move(validated),
        .error = ResolutionError::none,
        .rejected_index = std::nullopt,
        .address_error = AddressError::none,
    };
}

} // namespace mediaproxy::http
