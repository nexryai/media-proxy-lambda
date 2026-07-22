#pragma once

#include <cstdint>

#include <mediaproxy/http/origin_download.hpp>

namespace mediaproxy::runtime {

inline constexpr std::uint64_t response_submission_reserve_ms = 1'000;

using EpochMillisecondsFunction = std::uint64_t (*)(void* context) noexcept;

struct EpochClockApi {
    void* context = nullptr;
    EpochMillisecondsFunction now = nullptr;
};

[[nodiscard]] std::uint64_t system_epoch_milliseconds(void*) noexcept;

class InvocationDeadline final {
public:
    explicit InvocationDeadline(
        std::uint64_t deadline_ms,
        EpochClockApi clock = {
            .context = nullptr,
            .now = &system_epoch_milliseconds,
        }) noexcept;

    [[nodiscard]] long remaining_origin_milliseconds() const noexcept;
    [[nodiscard]] http::OriginTimeoutApi origin_timeout() noexcept;

private:
    static long remaining(void* context) noexcept;

    std::uint64_t deadline_ms_ = 0;
    EpochClockApi clock_;
};

} // namespace mediaproxy::runtime
