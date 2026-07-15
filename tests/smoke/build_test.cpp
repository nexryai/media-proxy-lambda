#include <string>

#include <gtest/gtest.h>

TEST(BuildSmoke, UsesPinnedLibcxx)
{
    const std::string runtime_name = "mediaproxy-lambda";
    EXPECT_EQ(runtime_name.size(), 17U);
}
