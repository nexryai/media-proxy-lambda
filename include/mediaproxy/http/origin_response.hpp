#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mediaproxy::http {

inline constexpr std::size_t maximum_origin_body_bytes = 10U * 1024U * 1024U;

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
    [[nodiscard]] auto append_body(std::span<const std::byte> bytes) noexcept -> std::size_t;
    [[nodiscard]] auto finish(long status) noexcept -> bool;

    [[nodiscard]] auto error() const noexcept -> OriginResponseError;
    [[nodiscard]] auto content_length() const noexcept -> std::optional<std::int64_t>;
    [[nodiscard]] auto location() const noexcept -> const std::optional<std::string> &;
    [[nodiscard]] auto body() const noexcept -> const std::vector<std::byte> &;
    [[nodiscard]] auto at_body_limit() const noexcept -> bool;

  private:
    void set_content_length(std::string_view value) noexcept;

    OriginResponseError error_ = OriginResponseError::none;
    std::optional<std::int64_t> content_length_;
    std::optional<std::string> location_;
    std::vector<std::byte> body_;
};

} // namespace mediaproxy::http
