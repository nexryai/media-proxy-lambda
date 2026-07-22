#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace mediaproxy::runtime {

inline constexpr std::size_t maximum_runtime_header_bytes = 64U * 1024U;
inline constexpr std::size_t maximum_runtime_event_bytes = 8U * 1024U * 1024U;

struct Invocation {
    std::string request_id;
    std::uint64_t deadline_ms = 0;
    std::string trace_id;
    std::vector<std::byte> event;
};

enum class NextParseStatus {
    incomplete,
    complete,
    error,
};

class NextResponseParser {
public:
    [[nodiscard]] NextParseStatus feed(std::span<const std::byte> bytes);
    [[nodiscard]] NextParseStatus status() const noexcept { return status_; }
    [[nodiscard]] const Invocation& invocation() const noexcept
    {
        return invocation_;
    }

private:
    [[nodiscard]] bool parse_headers(std::size_t header_end);

    NextParseStatus status_ = NextParseStatus::incomplete;
    std::vector<std::byte> buffer_;
    std::size_t body_offset_ = 0;
    std::size_t content_length_ = 0;
    Invocation invocation_;
};

[[nodiscard]] std::string make_next_request_head(
    std::string_view runtime_authority);

} // namespace mediaproxy::runtime
