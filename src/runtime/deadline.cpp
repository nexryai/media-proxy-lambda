#include <mediaproxy/runtime/deadline.hpp>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <limits>

#include <mediaproxy/http/origin_download.hpp>

namespace mediaproxy::runtime {

std::uint64_t system_epoch_milliseconds(void*) noexcept
{
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const auto milliseconds =
        std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
    return milliseconds > 0 ? static_cast<std::uint64_t>(milliseconds) : 0;
}

InvocationDeadline::InvocationDeadline(
    std::uint64_t deadline_ms,
    EpochClockApi clock) noexcept
    : deadline_ms_(deadline_ms)
    , clock_(clock)
{
}

long InvocationDeadline::remaining_origin_milliseconds() const noexcept
{
    if (clock_.now == nullptr) {
        return 0;
    }
    const std::uint64_t now = clock_.now(clock_.context);
    if (deadline_ms_ <= now
        || deadline_ms_ - now <= response_submission_reserve_ms) {
        return 0;
    }
    const std::uint64_t remaining =
        deadline_ms_ - now - response_submission_reserve_ms;
    return static_cast<long>(std::min<std::uint64_t>(
        remaining, static_cast<std::uint64_t>(std::numeric_limits<long>::max())));
}

http::OriginTimeoutApi InvocationDeadline::origin_timeout() noexcept
{
    return {
        .context = this,
        .remaining_milliseconds = &InvocationDeadline::remaining,
    };
}

long InvocationDeadline::remaining(void* context) noexcept
{
    if (context == nullptr) {
        return 0;
    }
    return static_cast<InvocationDeadline*>(context)
        ->remaining_origin_milliseconds();
}

} // namespace mediaproxy::runtime
