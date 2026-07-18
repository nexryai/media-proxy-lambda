#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <mediaproxy/http/address_policy.hpp>

namespace mediaproxy::http {

enum class ResolutionError {
    none,
    empty_answer,
    forbidden_address,
};

struct ResolutionPolicyResult {
    std::vector<ValidatedAddress> addresses;
    ResolutionError error = ResolutionError::none;
    std::optional<std::size_t> rejected_index;
    AddressError address_error = AddressError::none;

    [[nodiscard]] explicit operator bool() const noexcept
    {
        return error == ResolutionError::none;
    }
};

[[nodiscard]] ResolutionPolicyResult validate_resolved_addresses(
    std::span<const std::string_view> candidates);

} // namespace mediaproxy::http
