#include <gtest/gtest.h>
#include <mediaproxy/media/vips_runtime.hpp>
#include <vips/vips.h>

namespace {

using mediaproxy::media::initialize_vips;
using mediaproxy::media::vips_cache_maximum_entries;
using mediaproxy::media::vips_cache_maximum_files;
using mediaproxy::media::vips_cache_maximum_memory;
using mediaproxy::media::vips_worker_concurrency;

TEST(VipsRuntime, InitializesOnceWithBoundedExecutionSettings)
{
    ASSERT_TRUE(initialize_vips()) << vips_error_buffer();
    EXPECT_TRUE(initialize_vips());
    EXPECT_EQ(vips_concurrency_get(), vips_worker_concurrency);
    EXPECT_EQ(vips_cache_get_max_mem(), vips_cache_maximum_memory);
    EXPECT_EQ(vips_cache_get_max(), vips_cache_maximum_entries);
    EXPECT_EQ(vips_cache_get_max_files(), vips_cache_maximum_files);
}

} // namespace
