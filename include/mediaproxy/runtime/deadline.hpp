#pragma once

#include <cstdint>

#include <mediaproxy/http/origin_download.hpp>

namespace mediaproxy::runtime {

inline constexpr std::uint64_t response_submission_reserve_ms = 1'000;

using EpochMillisecondsFunction = std::uint64_t (*)(void *context) noexcept;

struct EpochClockApi {
    void *context = nullptr;
    EpochMillisecondsFunction now = nullptr;
};

[[nodiscard]] auto system_epoch_milliseconds(void * /*unused*/) noexcept -> std::uint64_t;

class InvocationDeadline final {
  public:
    explicit InvocationDeadline(std::uint64_t deadline_ms, EpochClockApi clock = {
                                                               .context = nullptr,
                                                               .now = &system_epoch_milliseconds,
                                                           }) noexcept;

    [[nodiscard]] auto remaining_origin_milliseconds() const noexcept -> long;
    [[nodiscard]] auto origin_timeout() noexcept -> http::OriginTimeoutApi;

  private:
    static auto remaining(void *context) noexcept -> long;

    std::uint64_t deadline_ms_ = 0;
    EpochClockApi clock_;
};

} // namespace mediaproxy::runtime
