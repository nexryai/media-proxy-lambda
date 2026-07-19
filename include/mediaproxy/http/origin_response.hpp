#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mediaproxy::http {

inline constexpr std::size_t maximum_origin_body_bytes =
    10U * 1024U * 1024U;

enum class OriginResponseError {
    none,
    invalid_content_length,
    content_length_too_large,
    blocked_by_nextdns,
    allocation,
    non_200_status,
};

class OriginResponseAccumulator final {
public:
    void consume_header_line(std::string_view line) noexcept;
    [[nodiscard]] std::size_t append_body(
        std::span<const std::byte> bytes) noexcept;
    [[nodiscard]] bool finish(long status) noexcept;

    [[nodiscard]] OriginResponseError error() const noexcept;
    [[nodiscard]] std::optional<std::int64_t> content_length() const noexcept;
    [[nodiscard]] const std::optional<std::string>& location() const noexcept;
    [[nodiscard]] const std::vector<std::byte>& body() const noexcept;
    [[nodiscard]] bool at_body_limit() const noexcept;

private:
    void set_content_length(std::string_view value) noexcept;

    OriginResponseError error_ = OriginResponseError::none;
    std::optional<std::int64_t> content_length_;
    std::optional<std::string> location_;
    std::vector<std::byte> body_;
};

} // namespace mediaproxy::http
