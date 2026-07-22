#include <cstdint>

#include <gtest/gtest.h>
#include <mediaproxy/runtime/deadline.hpp>

namespace {

using mediaproxy::runtime::EpochClockApi;
using mediaproxy::runtime::InvocationDeadline;

std::uint64_t Now(void* context) noexcept
{
    return *static_cast<std::uint64_t*>(context);
}

TEST(RuntimeDeadline, SubtractsExactResponseSubmissionReserve)
{
    std::uint64_t now = 10'000;
    InvocationDeadline deadline{
        15'000, EpochClockApi{.context = &now, .now = &Now}};
    EXPECT_EQ(deadline.remaining_origin_milliseconds(), 4'000);

    now = 13'999;
    EXPECT_EQ(deadline.remaining_origin_milliseconds(), 1);
    now = 14'000;
    EXPECT_EQ(deadline.remaining_origin_milliseconds(), 0);
    now = 16'000;
    EXPECT_EQ(deadline.remaining_origin_milliseconds(), 0);
}

TEST(RuntimeDeadline, RefreshesTimeThroughOriginCallback)
{
    std::uint64_t now = 1'000;
    InvocationDeadline deadline{
        5'000, EpochClockApi{.context = &now, .now = &Now}};
    const auto timeout = deadline.origin_timeout();
    ASSERT_NE(timeout.remaining_milliseconds, nullptr);
    EXPECT_EQ(timeout.remaining_milliseconds(timeout.context), 3'000);
    now = 3'500;
    EXPECT_EQ(timeout.remaining_milliseconds(timeout.context), 500);
}

TEST(RuntimeDeadline, RejectsMissingClock)
{
    InvocationDeadline deadline{5'000, {}};
    EXPECT_EQ(deadline.remaining_origin_milliseconds(), 0);
}

} // namespace
